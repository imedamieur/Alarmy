#include <pitches.h>

#include <LCD-I2C.h>
#include <pitches.h>
#include <TimeLib.h>
#include <WiFiNINA.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>
/* Typical pin layout used:
 * -----------------------------------------------------------------------------------------
 *             MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
 *             Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
 * Signal      Pin          Pin           Pin       Pin        Pin              Pin
 * -----------------------------------------------------------------------------------------
 * RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
 * SPI SS      SDA(SS)      10            53        D10        10               10
 * SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
 * SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
 * SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15
*/


#define RST_PIN         6          // Configurable, see typical pin layout above
#define SS_PIN          7         // Configurable, see typical pin layout above

#define BUZZER_PIN 3 

//RFID 

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

// lCD
LCD_I2C lcd(0x27, 16, 2); // Default address of most PCF8574 modules, change according
const char *daysOfTheWeek[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
long previousMillis = 0;
long previousMinute = 0;
const long interval =  1000; // Update interval in milliseconds
int current_day = 0;

// Music parameters for the buzzer
int melody[] = {
  NOTE_E5, NOTE_B4, NOTE_C5, NOTE_D5, NOTE_C5, NOTE_B4,
  NOTE_A4, NOTE_A4, NOTE_C5, NOTE_E5, NOTE_D5, NOTE_C5,
  NOTE_B4, NOTE_C5, NOTE_D5, NOTE_E5,
  NOTE_C5, NOTE_A4, NOTE_A4, NOTE_A4, NOTE_B4, NOTE_C5,
  
  NOTE_D5, NOTE_F5, NOTE_A5, NOTE_G5, NOTE_F5,
  NOTE_E5, NOTE_C5, NOTE_E5, NOTE_D5, NOTE_C5,
  NOTE_B4, NOTE_B4, NOTE_C5, NOTE_D5, NOTE_E5,
  NOTE_C5, NOTE_A4, NOTE_A4, REST, 
  
  NOTE_E5, NOTE_B4, NOTE_C5, NOTE_D5, NOTE_C5, NOTE_B4,
  NOTE_A4, NOTE_A4, NOTE_C5, NOTE_E5, NOTE_D5, NOTE_C5,
  NOTE_B4, NOTE_C5, NOTE_D5, NOTE_E5,
  NOTE_C5, NOTE_A4, NOTE_A4, NOTE_A4, NOTE_B4, NOTE_C5,
  
  NOTE_D5, NOTE_F5, NOTE_A5, NOTE_G5, NOTE_F5,
  NOTE_E5, NOTE_C5, NOTE_E5, NOTE_D5, NOTE_C5,
  NOTE_B4, NOTE_B4, NOTE_C5, NOTE_D5, NOTE_E5,
  NOTE_C5, NOTE_A4, NOTE_A4, REST, 
  
  NOTE_E5, NOTE_C5,
  NOTE_D5, NOTE_B4,
  NOTE_C5, NOTE_A4,
  NOTE_GS4, NOTE_B4, REST, 
  NOTE_E5, NOTE_C5,
  NOTE_D5, NOTE_B4,
  NOTE_C5, NOTE_E5, NOTE_A5,
  NOTE_GS5
};

int durations[] = {
  4, 8, 8, 4, 8, 8,
  4, 8, 8, 4, 8, 8,
  4, 8, 4, 4,
  4, 4, 8, 4, 8, 8,
  
  4, 8, 4, 8, 8,
  4, 8, 4, 8, 8,
  4, 8, 8, 4, 4,
  4, 4, 4, 4,
  
  4, 8, 8, 4, 8, 8,
  4, 8, 8, 4, 8, 8,
  4, 8, 4, 4,
  4, 4, 8, 4, 8, 8,
  
  4, 8, 4, 8, 8,
  4, 8, 4, 8, 8,
  4, 8, 8, 4, 4,
  4, 4, 4, 4,
  
  2, 2,
  2, 2,
  2, 2,
  2, 4, 8, 
  2, 2,
  2, 2,
  4, 4, 2,
  2
};

//WIFI/JSON
JsonDocument doc;

char JSON[3000];
char ssid[] = "The one who left it all behind";        
char pass[] = "akaishuichihh";    
int status = WL_IDLE_STATUS;    

//JSON data
int chargingTime;
char dateTime[20] = "";
int alarm_hour[31];
int alarm_minute[31];
int alarm_day[31];
int alarm_month[31];

WiFiClient client;

void setup() {
    Serial.begin(9600);		// Initialize serial communications with the PC
     while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
    }
    // this stop the library(LCD-I2C) from calling Wire.begin()
    lcd.begin();
    lcd.display();
    lcd.backlight();
    // Buzzer Setup 
    pinMode(BUZZER_PIN, OUTPUT); 
    // RFID Setup
    SPI.begin();			// Init SPI bus
	  mfrc522.PCD_Init();		// Init MFRC522
	  delay(4);				// Optional delay. Some board do need more time after init to be ready, see Readme
    mfrc522.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details
	  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
    //WIFI
    lcdPrint("Connecting to","WIFI...");
    connect_wifi();
    while(chargingTime==NULL){
      lcdPrint("Retrieving alarm","data...");
      get_alarm_data();
    }
    INIT_CLOCK();
    lcd.clear();
}

void lcdPrint(char* message1,char* message2){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(message1);
  lcd.setCursor(0,1);
  lcd.print(message2);
}

void alarmPlaying()
{
  int size = sizeof(durations) / sizeof(int); 
  int note = 0;
  lcdPrint("Alarm ringing","Scan to dismiss");
	 while (note < size && !isCardScanned()) { 
	   //to calculate the note duration, take one second divided by the note type. 
	   //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc. 
	   int duration = 1000 / durations[note]; 
	   tone(BUZZER_PIN, melody[note], duration); 
	   //to distinguish the notes, set a minimum time between them. 
	   //the note's duration + 30% seems to work well: 
	   int pauseBetweenNotes = duration * 1.30; 
	   delay(pauseBetweenNotes); 
	   //stop the tone playing: 
	   noTone(BUZZER_PIN); 
     note++;
     if( note == size) note = 0;
	 }
  lcd.clear(); 
}
void INIT_CLOCK()
{
  // Define variables to store parsed values
  int year, month, day, hour, minute, second;
  
  // Parse the date and time string
  sscanf(dateTime, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);
  setTime(hour, minute, second, day, month, year);

}

bool isCardScanned() {
	// Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
	if (  mfrc522.PICC_IsNewCardPresent()) {
      Serial.println("card hh");
		  return true;
	} else {
      Serial.println("no card hh");
		  return false;
	}
}

bool isAlarmTime(int minute,int hour,int day,int month){

  for(int i=1;i<31;i++){
    if ((minute==alarm_minute[i]) && (hour==alarm_hour[i]) && (day==alarm_day[i]) && (month==alarm_month[i])){
      return true;
    }
  }
  return false;
}

void LCD_LOOP()
{
  long currentMillis = millis() ;
  
  time_t currentTime = now(); // Get current time
    tmElements_t tm;
    breakTime(currentTime, tm);

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    

    lcd.setCursor(0, 1);
    lcd.print(tm.Hour);
    lcd.print(':');
    if (tm.Minute < 10) lcd.print('0'); // Adding leading zero for minutes < 10
    lcd.print(tm.Minute);
    lcd.print(':');
    if (tm.Second < 10) lcd.print('0'); // Adding leading zero for seconds < 10
    lcd.print(tm.Second);
    lcd.print("   ");
    lcd.setCursor(0, 0);
    lcd.print(daysOfTheWeek[weekday(currentTime) - 1]);
    lcd.print(" ,");
    lcd.print(tm.Day);
    lcd.print('/');
    lcd.print(tm.Month);
    lcd.print('/');
    lcd.print(tmYearToCalendar(tm.Year));
    if (tm.Minute - previousMinute != 0){
    previousMinute = tm.Minute;
    if (isAlarmTime(tm.Minute,tm.Hour,tm.Day,tm.Month)){
      alarmPlaying();
    }
  }
  }
}

void connect_wifi(){
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }


  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }

  // you're connected now, so print out the data:
  Serial.print("You're connected to the network");
}

void get_alarm_data(){

  if(client.connect(WiFi.gatewayIP(), 4040)) {
    Serial.println("Connected to server");
    client.println("GET / HTTP/1.1");
    client.print("Host: ");
    client.println(WiFi.gatewayIP());
    client.println("Connection: close");
    client.println();

    // Wait for response
    bool jsonStarted = false;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if(c == '{'){
          JSON[0] = c;
          int parentheses = 1;
          int i = 1;
          while(parentheses){
           
            c = client.read();
            JSON[i] = c;
            i++;
            if(c == '{') parentheses++;
            if(c == '}') parentheses--;
          }
        }
      }
    }

    // Close connection
    client.stop();
    Serial.println("\nConnection closed");
    Serial.println("Desrialization");
    DeserializationError error = deserializeJson(doc, JSON, 3000);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    int chargingTime = doc["chargingTime"]; // 1
    const char* dateTime2 = doc["dateTime"]; // "2024-02-14 11:35:44"
    strcpy(dateTime,dateTime2);
   
   
    int k = 0;
    for (JsonObject alarm : doc["alarms"].as<JsonArray>()) {
      alarm_hour[k] = alarm["hour"];
      alarm_minute[k] = alarm["minute"];

      alarm_day[k] = alarm["day"];
      alarm_month[k] = alarm["month"];
      k++;
    }
   
    Serial.print("Charging time: ");
    Serial.println(chargingTime);

    Serial.print("Date and time: ");
    Serial.println(dateTime);

    // FOR TESTING PURPOSES ONLY
    alarm_hour[1]=13;
    alarm_minute[1]=48;
    alarm_hour[2]=11;
    alarm_minute[2]=23;
    alarm_day[2]=alarm_day[1];


    Serial.println("Alarms: ");
    for(int i=1;i<31;i++){
      Serial.print(alarm_hour[i]);
      Serial.print(":");
      Serial.println(alarm_minute[i]);
    }


  } else {
    Serial.println("Connection to server failed");
  }
}

void loop()
{   
  LCD_LOOP();
  
}
