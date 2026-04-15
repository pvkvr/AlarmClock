#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <TM1637Display.h>
#include <OneWire.h>
#include <DallasTemperature.h>


#define CLK 7
#define DIO 6
#define BTN1 2
#define BTN2 3
#define BTN3 4
#define BUZZER 10
#define LED_TIME A3
#define LED_TEMP A2
#define LED_ALARM A1
#define TEMP_PIN 9


RTC_DS1307 rtc;
TM1637Display display(CLK, DIO);
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);


enum Mode { TIME, TEMPERATURE, MENU, SET_ALARM, SET_TIME };
Mode currentMode = TIME;

int alarmH = 7, alarmM = 0;
int tempH = 0, tempM = 0;
int menuSelection = 1;
bool alarmEnabled = false;
bool alarmRinging = false;
bool editingHour = true;

bool lastBtn1 = HIGH;
bool lastBtn2 = HIGH;
bool lastBtn3 = HIGH;

unsigned long lastActivityTime = 0;
unsigned long alarmStartTime = 0;

void setup()
{
  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);
  pinMode(LED_TIME, OUTPUT);
  pinMode(LED_TEMP, OUTPUT);
  pinMode(LED_ALARM, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  
  digitalWrite(LED_TIME, HIGH);
  digitalWrite(LED_TEMP, LOW);
  digitalWrite(LED_ALARM, LOW);
  
  display.setBrightness(0x0f);
  sensors.begin();
  
  if (!rtc.begin())
  {
    display.showNumberDec(8888, false);
    while (1);
  }
  
  if (!rtc.isrunning())
  {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  delay(100);
  lastBtn1 = digitalRead(BTN1);
  lastBtn2 = digitalRead(BTN2);
  lastBtn3 = digitalRead(BTN3);
}

void beepConfirm()
{
  tone(BUZZER, 2000, 50);
  delay(60);
  tone(BUZZER, 2000, 50);
  delay(60);
}

void handleButtons()
{
  bool btn1 = digitalRead(BTN1);
  bool btn2 = digitalRead(BTN2);
  bool btn3 = digitalRead(BTN3);
  
  if (alarmRinging && lastBtn3 == HIGH && btn3 == LOW)
  {
    alarmRinging = false;
    noTone(BUZZER);
    lastBtn3 = btn3;
    return;
  }
  

  if (lastBtn1 == HIGH && btn1 == LOW)
  {
    if (currentMode == SET_ALARM || currentMode == SET_TIME)
    {
      if (currentMode == SET_ALARM)
      {
        if (editingHour)
          alarmH = (alarmH - 1 + 24) % 24;
        else
          alarmM = (alarmM - 1 + 60) % 60;
      }
      else
      {
        if (editingHour)
          tempH = (tempH - 1 + 24) % 24;
        else
          tempM = (tempM - 1 + 60) % 60;
      }
      lastActivityTime = millis();
      delay(150);
    }
    else if (currentMode == TIME || currentMode == TEMPERATURE)
    {
      currentMode = (currentMode == TIME) ? TEMPERATURE : TIME;
      delay(50);
    }
  }
  
  if (lastBtn2 == HIGH && btn2 == LOW)
  {
    if (currentMode == MENU)
    {
      menuSelection = (menuSelection == 1) ? 2 : 1;
      tone(BUZZER, 1500, 30);
      delay(100);
    }
    else if (currentMode == SET_ALARM || currentMode == SET_TIME)
    {
      editingHour = !editingHour;
      lastActivityTime = millis();
      delay(50);
    }
    else if (currentMode == TIME || currentMode == TEMPERATURE)
    {
      currentMode = MENU;
      menuSelection = 1;
      lastActivityTime = millis();
      tone(BUZZER, 2000, 50);
      delay(50);
    }
  }
  

  if (lastBtn3 == HIGH && btn3 == LOW)
  {
    if (currentMode == MENU)
    {
      if (menuSelection == 1)
      {
        currentMode = SET_ALARM;
      }
      else
      {
        DateTime now = rtc.now();
        tempH = now.hour();
        tempM = now.minute();
        currentMode = SET_TIME;
      }
      editingHour = true;
      lastActivityTime = millis();
      beepConfirm();
      delay(50);
    }
    else if (currentMode == SET_ALARM || currentMode == SET_TIME)
    {
      if (currentMode == SET_ALARM)
      {
        if (editingHour)
          alarmH = (alarmH + 1) % 24;
        else
          alarmM = (alarmM + 1) % 60;
      }
      else
      {
        if (editingHour)
          tempH = (tempH + 1) % 24;
        else
          tempM = (tempM + 1) % 60;
      }
      lastActivityTime = millis();
      delay(150);
    }
    else
    {
      alarmEnabled = !alarmEnabled;
      tone(BUZZER, alarmEnabled ? 2500 : 1500, 100);
      delay(50);
    }
  }
  
  lastBtn1 = btn1;
  lastBtn2 = btn2;
  lastBtn3 = btn3;
}

void showTimeValue(int h, int m)
{
  bool blink = (millis() / 500) % 2 == 0;
  int val = (h * 100) + m;
  
  if (blink)
  {
    display.showNumberDecEx(val, 0b01000000, true);
  }
  else
  {
    uint8_t data[4];
    if (editingHour)
    {
      data[0] = 0x00;
      data[1] = 0x00;
      data[2] = display.encodeDigit(m / 10);
      data[3] = display.encodeDigit(m % 10);
    }
    else
    {
      data[0] = display.encodeDigit(h / 10);
      data[1] = display.encodeDigit(h % 10);
      data[2] = 0x00;
      data[3] = 0x00;
    }
    display.setSegments(data);
  }
}

void checkAlarm()
{
  DateTime now = rtc.now();
  
  if (alarmEnabled && !alarmRinging && 
      now.hour() == alarmH && now.minute() == alarmM && now.second() == 0)
  {
    alarmRinging = true;
    alarmStartTime = millis();
  }
  
  if (alarmRinging)
  {
    if ((millis() / 300) % 2 == 0)
      tone(BUZZER, 1000);
    else
      noTone(BUZZER);
  }
}

void updateLEDs()
{
  bool ledBlink = (millis() / 400) % 2 == 0;
  
  if (currentMode == MENU)
  {
    digitalWrite(LED_TIME, ledBlink);
    digitalWrite(LED_TEMP, ledBlink);
    digitalWrite(LED_ALARM, ledBlink);
  }
  else if (currentMode == SET_ALARM)
  {
    digitalWrite(LED_TIME, LOW);
    digitalWrite(LED_TEMP, LOW);
    digitalWrite(LED_ALARM, ledBlink);
  }
  else if (currentMode == SET_TIME)
  {
    digitalWrite(LED_TIME, ledBlink);
    digitalWrite(LED_TEMP, LOW);
    digitalWrite(LED_ALARM, LOW);
  }
  else
  {
    digitalWrite(LED_TIME, currentMode == TIME);
    digitalWrite(LED_TEMP, currentMode == TEMPERATURE);
    digitalWrite(LED_ALARM, alarmEnabled);
  }
}

void loop()
{
  handleButtons();
  checkAlarm();
  
  if (currentMode == MENU && millis() - lastActivityTime > 5000)
    currentMode = TIME;
  
  if (currentMode == SET_ALARM && millis() - lastActivityTime > 10000)
  {
    currentMode = TIME;
    alarmEnabled = true;
    beepConfirm();
  }
  
  if (currentMode == SET_TIME && millis() - lastActivityTime > 10000)
  {
    rtc.adjust(DateTime(2025, 1, 1, tempH, tempM, 0));
    currentMode = TIME;
    beepConfirm();
  }
  
  updateLEDs();
  
  if (currentMode == TIME)
  {
    DateTime now = rtc.now();
    int val = (now.hour() * 100) + now.minute();
    uint8_t colon = (now.second() % 2 == 0) ? 0b01000000 : 0b00000000;
    display.showNumberDecEx(val, colon, true);
  }
  else if (currentMode == TEMPERATURE)
  {
    sensors.requestTemperatures();
    int temp = (int)sensors.getTempCByIndex(0);
    uint8_t data[4];
    
    if (temp >= 10 || temp <= -10)
      data[0] = display.encodeDigit(abs(temp) / 10);
    else if (temp < 0)
      data[0] = 0x40;  
    else
      data[0] = 0x00;
    
    data[1] = display.encodeDigit(abs(temp) % 10);
    data[2] = 0x63;
    data[3] = 0x39;  
    display.setSegments(data);
  }
  else if (currentMode == MENU)
  {
    if ((millis() / 500) % 2 == 0)
      display.showNumberDec(menuSelection, false);
    else
      display.clear();
  }
  else if (currentMode == SET_ALARM)
  {
    showTimeValue(alarmH, alarmM);
  }
  else if (currentMode == SET_TIME)
  {
    showTimeValue(tempH, tempM);
  }
  
  delay(50);
}