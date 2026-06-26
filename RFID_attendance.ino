#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <vector>

// RFID pins
#define SS_PIN 5
#define RST_PIN 4
MFRC522 rfid(SS_PIN, RST_PIN);

// OLED setup
#define SCR_W 128
#define SCR_H 64
Adafruit_SSD1306 oled(SCR_W, SCR_H, &Wire, -1);

// my wifi details
const char* ssid     = "YOU USERNAME";
const char* password = "YOUR PASSWORD";

// google sheet web app
String sheetURL = "YOUR WEBAPP URL";
String mySection = "B";

// store records when wifi is off
struct AttendRecord {
  String roll;
  String cardUID;
  String stdName;
  String scanDate;
  String scanDay;
  String scanTime;
};

std::vector<AttendRecord> savedRecords;
bool isWifiOn = false;
unsigned long wifiCheckTimer = 0;

// draw a single wifi arc
void drawArc(int cx, int cy, int r)
{
  for(int i = -r; i <= r; i++)
  {
    int yOff = (i * i) / (r * 2);
    oled.drawPixel(cx + i, cy - r + yOff, SSD1306_WHITE);
  }
}

// draw wifi icon with signal level 0-3
void drawWiFiIcon(int cx, int cy, int bars)
{
  oled.fillCircle(cx, cy, 2, SSD1306_WHITE);
  if(bars >= 1) drawArc(cx, cy - 2, 6);
  if(bars >= 2) drawArc(cx, cy - 4, 10);
  if(bars >= 3) drawArc(cx, cy - 6, 14);
}

void showConnecting(int f)
{
  oled.clearDisplay();
  drawWiFiIcon(64, 40, f % 4);
  oled.setCursor(25, 55);
  oled.print("Connecting...");
  oled.display();
}

void showConnected()
{
  oled.clearDisplay();
  drawWiFiIcon(64, 40, 3);
  oled.setCursor(40, 55);
  oled.print("Connected");
  oled.display();
  delay(1200);
}

void showNoWifi()
{
  oled.clearDisplay();
  drawWiFiIcon(64, 40, 3);
  // cross over the icon
  oled.drawLine(48, 14, 80, 46, SSD1306_WHITE);
  oled.drawLine(80, 14, 48, 46, SSD1306_WHITE);
  oled.setCursor(10, 55);
  oled.print("WiFi Not Connected");
  oled.display();
  delay(2000);
}

// animated scan screen
void showScanScreen(int f)
{
  oled.clearDisplay();
  oled.drawRect(54, 20, 20, 14, SSD1306_WHITE);
  oled.drawLine(58, 24, 70, 24, SSD1306_WHITE);
  int w = f % 3;
  oled.drawCircle(64, 27, 8  + w * 3, SSD1306_WHITE);
  oled.drawCircle(64, 27, 14 + w * 3, SSD1306_WHITE);
  oled.setCursor(28, 54);
  oled.print("Scan Your Card");
  oled.display();
}

void showUnknownCard()
{
  oled.clearDisplay();
  oled.drawCircle(64, 22, 16, SSD1306_WHITE);
  oled.drawLine(56, 14, 72, 30, SSD1306_WHITE);
  oled.drawLine(72, 14, 56, 30, SSD1306_WHITE);
  oled.setCursor(26, 50);
  oled.println("Unknown Card");
  oled.display();
  delay(2000);
}

void showWelcome(String stdName)
{
  oled.clearDisplay();
  oled.setCursor(40, 44);
  oled.println("Welcome");
  oled.setCursor(20, 56);
  oled.println(stdName);
  oled.display();
  delay(1500);
}

void showStudentInfo(String stdName, String roll, String sec, String t)
{
  oled.clearDisplay();
  oled.setCursor(0, 10);
  oled.print("Name : "); oled.println(stdName);
  oled.setCursor(0, 25);
  oled.print("Roll : "); oled.println(roll);
  oled.setCursor(0, 40);
  oled.print("Sec  : "); oled.println(sec);
  oled.setCursor(0, 55);
  oled.print("Time : "); oled.println(t);
  oled.display();
  delay(2000);
}

void sendToSheet(AttendRecord &rec)
{
  HTTPClient http;
  http.begin(sheetURL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String data = "rollno=" + rec.roll +
                "&uid="   + rec.cardUID +
                "&name="  + rec.stdName +
                "&sec="   + mySection +
                "&date="  + rec.scanDate +
                "&day="   + rec.scanDay +
                "&time="  + rec.scanTime;

  http.POST(data);
  http.end();
}

void setup()
{
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  Wire.begin(21, 22);
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  // try connecting wifi for 7 seconds
  WiFi.begin(ssid, password);
  unsigned long t0 = millis();
  int f = 0;
  while(WiFi.status() != WL_CONNECTED && millis() - t0 < 7000)
  {
    showConnecting(f++);
    delay(400);
  }

  if(WiFi.status() == WL_CONNECTED)
  {
    isWifiOn = true;
    showConnected();
  }
  else
  {
    isWifiOn = false;
    showNoWifi();
  }

  // IST timezone = UTC+5:30 = 19800 seconds offset
  configTime(19800, 0, "time.google.com");
}

void loop()
{
  static int frameCount = 0;
  showScanScreen(frameCount++);
  delay(80);

  // check if wifi dropped
  if(WiFi.status() != WL_CONNECTED && isWifiOn)
  {
    isWifiOn = false;
    showNoWifi();
  }

  // retry wifi every 10 seconds
  if(!isWifiOn && millis() - wifiCheckTimer > 10000)
  {
    WiFi.begin(ssid, password);
    wifiCheckTimer = millis();
  }

  // wifi came back
  if(WiFi.status() == WL_CONNECTED && !isWifiOn)
  {
    isWifiOn = true;
    showConnected();
  }

  // upload any saved offline records
  if(isWifiOn && savedRecords.size() > 0)
  {
    for(auto &r : savedRecords)
    {
      sendToSheet(r);
      delay(300);
    }
    savedRecords.clear();
  }

  // wait for card
  if(!rfid.PICC_IsNewCardPresent()) return;
  if(!rfid.PICC_ReadCardSerial())   return;

  // read card UID
  String uid = "";
  for(byte i = 0; i < rfid.uid.size; i++)
  {
    if(rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  // match uid to student
  String stdName = "Unknown";
  String roll    = "";

  if(uid == "YOUR_RFID_UID")
  {
    stdName = "WHAT_NAME_YOU_NEED_TO_ASSIGN";
    roll    = "ASSIGN_AS_YOU_WANT";
  }
  else if(uid == "YOUR_RFID_UID")
  {
    stdName = "WHAT_NAME_YOU_NEED_TO_ASSIGN";
    roll    = "ASSIGN_AS_YOU_WANT";
  }

  if(stdName == "Unknown")
  {
    showUnknownCard();
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  // get current time
  String dateStr = "OFFLINE";
  String dayStr  = "OFFLINE";
  String timeStr = "OFFLINE";

  if(isWifiOn)
  {
    struct tm tInfo;
    if(getLocalTime(&tInfo))
    {
      char d[15], da[12], t[10];
      strftime(d,  sizeof(d),  "%d-%m-%Y", &tInfo);
      strftime(da, sizeof(da), "%A",       &tInfo);
      strftime(t,  sizeof(t),  "%H:%M:%S", &tInfo);
      dateStr = d;
      dayStr  = da;
      timeStr = t;
    }
  }

  showWelcome(stdName);
  showStudentInfo(stdName, roll, mySection, timeStr);

  // build record
  AttendRecord rec;
  rec.roll     = roll;
  rec.cardUID  = uid;
  rec.stdName  = stdName;
  rec.scanDate = dateStr;
  rec.scanDay  = dayStr;
  rec.scanTime = timeStr;

  if(isWifiOn)
  {
    HTTPClient http;
    http.begin(sheetURL);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String data = "rollno=" + rec.roll +
                  "&uid="   + rec.cardUID +
                  "&name="  + rec.stdName +
                  "&sec="   + mySection +
                  "&date="  + rec.scanDate +
                  "&day="   + rec.scanDay +
                  "&time="  + rec.scanTime;

    int code = http.POST(data);
    if(code <= 0)
      savedRecords.push_back(rec); // save if failed
    http.end();
  }
  else
  {
    savedRecords.push_back(rec); // save for later
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(3000);
}
