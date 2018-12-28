/*
 * Phase Order Indicator
 * 
 * Designed for Rizky Aldi Husain's Final Project
 * 
 * Created 25 July 2018
 * @Gorontalo, Indonesia
 * by ZulNs
 */


#define PHASE_1_SENSOR_PIN  0  // PC0 (A0)
#define PHASE_2_SENSOR_PIN  1  // PC1 (A1)
#define PHASE_3_SENSOR_PIN  2  // PC2 (A2)

#define LCD_RS_PIN  12
#define LCD_EN_PIN  11
#define LCD_D4_PIN  5
#define LCD_D5_PIN  4
#define LCD_D6_PIN  3
#define LCD_D7_PIN  2

#define FAILED_PHASE_ORDER  0
#define ORDERED_PHASE       1
#define UNORDERED_PHASE     2

#define AREF_PIN       0x00
#define INTERNAL_AVCC  0x40
#define INTERNAL_1V1   0xC0

#define NUMBER_OF_FULL_WAVE_SAMPLING  187  // about 179 sampling within 20ms (one cycle periode for 50Hz wave)
#define NUMBER_OF_HALF_WAVE_SAMPLING  94
#define STANDARD_VIN_ADC_VAL          207  // (207 * 1.5) = 311V = 220V * sqrt(2)
#define DELTA_VIN_ADC_VAL             38   // (38 * 1.5) = 57V; (57V / sqrt(2)) = ±40V
#define DELTA_ZC_VOLTAGE_ADC_VAL      12   // (8 * 1.5) = ±18V

#include <LiquidCrystal.h>

const uint16_t MAX_VIN_RANGE_ADC_VAL  = 2 * STANDARD_VIN_ADC_VAL + 2 * DELTA_VIN_ADC_VAL;
const uint16_t MIN_VIN_RANGE_ADC_VAL  = 2 * STANDARD_VIN_ADC_VAL - 2 * DELTA_VIN_ADC_VAL;
const uint16_t MAX_ZC_VOLTAGE_ADC_VAL = 512 + DELTA_ZC_VOLTAGE_ADC_VAL;
const uint16_t MIN_ZC_VOLTAGE_ADC_VAL = 512 - DELTA_ZC_VOLTAGE_ADC_VAL;

uint16_t interval[3];
uint8_t  oldSensorStatus = 0;
uint8_t  oldPhaseOrder = FAILED_PHASE_ORDER;

LiquidCrystal lcd(LCD_RS_PIN, LCD_EN_PIN, LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);

void setAdcReference(const uint8_t source)
{
  uint8_t tmp = ADMUX & 0x3F | source;
  ADMUX = tmp;
}

void setAdcChannel(const uint8_t channel)
{
  uint8_t tmp = ADMUX & 0xF0 | channel;
  ADMUX = tmp;
}

uint16_t readAdc()
{
  uint8_t high, low;
  bitSet(ADCSRA, ADSC);  // start conversion
  while (bitRead(ADCSRA, ADSC));
  low = ADCL;
  high = ADCH;
  return (high << 8) | low;
}

boolean checkVoltageSensor(const uint8_t adcChannel)
{
  uint16_t adcVal;
  uint16_t maxVal = 0;
  uint16_t minVal = 1023;
  uint16_t midVal;
  uint8_t  i = NUMBER_OF_FULL_WAVE_SAMPLING;
  setAdcChannel(adcChannel);
  readAdc();
  while (i > 0)
  {
    adcVal = readAdc();
    if (adcVal > maxVal)
    {
      maxVal = adcVal;
    }
    if (adcVal < minVal)
    {
      minVal = adcVal;
    }
    i--;
  }
  interval[adcChannel] = maxVal - minVal;
  if (interval[adcChannel] > MAX_VIN_RANGE_ADC_VAL || interval[adcChannel] < MIN_VIN_RANGE_ADC_VAL)
  {
    return false;
  }
  midVal = interval[adcChannel] / 2 + minVal;
  if (midVal > MAX_ZC_VOLTAGE_ADC_VAL || MIN_ZC_VOLTAGE_ADC_VAL > midVal)
  {
    return false;
  }
  return true;
}

boolean detectZeroCrossing(const uint8_t adcChannel)
{
  uint16_t adcVal;
  uint8_t  i = NUMBER_OF_HALF_WAVE_SAMPLING;
  setAdcChannel(adcChannel);
  readAdc();
  while (i > 0)
  {
    adcVal = readAdc();
    if (MAX_ZC_VOLTAGE_ADC_VAL >= adcVal && adcVal >= MIN_ZC_VOLTAGE_ADC_VAL)
    {
      return true;
    }
    i--;
  }
  return false;
}

uint8_t determinePhaseOrder()
{
  uint16_t phase1, phase3;
  if (!detectZeroCrossing(PHASE_2_SENSOR_PIN))
  {
    return FAILED_PHASE_ORDER;
  }
  delayMicroseconds(3300);
  setAdcChannel(PHASE_1_SENSOR_PIN);
  phase1 = readAdc();
  setAdcChannel(PHASE_3_SENSOR_PIN);
  phase3 = readAdc();
  if (512 + interval[0] / 12 >= phase1 && phase1 >= 512 - interval[0] / 12)
  {
    if (phase3 > 512 + interval[2] / 4 || 512 - interval[2] / 4 > phase3)
    {
      return ORDERED_PHASE;
    }
    return FAILED_PHASE_ORDER;
  }
  if (512 + interval[2] / 12 >= phase3 && phase3 >= 512 - interval[2] / 12)
  {
    if (phase1 > 512 + interval[0] / 4 || 512 - interval[0] / 4 > phase1)
    {
      return UNORDERED_PHASE;
    }
  }
  return FAILED_PHASE_ORDER;
}

void setup()
{
  lcd.begin(16, 2);
  //          "0123456789012345"
  lcd.print(F("P1:X  P2:X  P3:X"));
  lcd.setCursor(0, 1);
  lcd.print(F("Phase Order: ???"));
  
  setAdcReference(INTERNAL_AVCC);
  setAdcChannel(0);
  readAdc();
}

void loop()
{
  uint8_t sensorStatus = 0;
  uint8_t phaseOrder = FAILED_PHASE_ORDER;
  bitWrite(sensorStatus, 0, checkVoltageSensor(PHASE_1_SENSOR_PIN));
  bitWrite(sensorStatus, 1, checkVoltageSensor(PHASE_2_SENSOR_PIN));
  bitWrite(sensorStatus, 2, checkVoltageSensor(PHASE_3_SENSOR_PIN));
  if (bitRead(sensorStatus, 0) != bitRead(oldSensorStatus, 0))
  {
    lcd.setCursor(3, 0);
    if (bitRead(sensorStatus, 0))
    {
      lcd.print(F("O"));
    }
    else
    {
      lcd.print(F("X"));
    }
  }
  if (bitRead(sensorStatus, 1) != bitRead(oldSensorStatus, 1))
  {
    lcd.setCursor(9, 0);
    if (bitRead(sensorStatus, 1))
    {
      lcd.print(F("O"));
    }
    else
    {
      lcd.print(F("X"));
    }
  }
  if (bitRead(sensorStatus, 2) != bitRead(oldSensorStatus, 2))
  {
    lcd.setCursor(15, 0);
    if (bitRead(sensorStatus, 2))
    {
      lcd.print(F("O"));
    }
    else
    {
      lcd.print(F("X"));
    }
  }
  oldSensorStatus = sensorStatus;
  if (sensorStatus == 7)
  {
    phaseOrder = determinePhaseOrder();
  }
  if (phaseOrder != oldPhaseOrder)
  {
    lcd.setCursor(13, 1);
    if (phaseOrder == ORDERED_PHASE)
    {
      lcd.print(F("RST"));
    }
    else if (phaseOrder == UNORDERED_PHASE)
    {
      lcd.print(F("RTS"));
    }
    else // if (phaseOrder == FAILED_PHASE_ORDER)
    {
      lcd.print(F("???"));
    }
  }
  oldPhaseOrder = phaseOrder;
}

