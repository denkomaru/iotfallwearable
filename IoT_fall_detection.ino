#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME ""
#define BLYNK_AUTH_TOKEN ""

// Comment this out to disable prints and save space
#define BLYNK_PRINT Serial

#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

#include "Wire.h"
 
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

#include <Firebase_ESP_Client.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

//firebase start-----------------------------
/* 1. Define the WiFi credentials */
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// For the following credentials, see examples/Authentications/SignInAsUser/EmailPassword/EmailPassword.ino

/* 2. Define the API Key */
#define API_KEY ""

/* 3. Define the RTDB URL */
#define DATABASE_URL "" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

/* 4. Define the user Email and password that alreadey registerd or added in your project */
// Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

  // Variable to save USER UID
String uid;

  // Database main path (to be updated in setup with the user UID)
String databasePath;
  // Database child nodes
String acceleration = "/acceleration";
String orientation = "/orientation";
String one = "/count";

  // Parent Node (to be updated in every loop)
String parentPath;

FirebaseJson json;

unsigned long sendDataPrevMillis = 0;

unsigned long count = 0;

bool signupOK = false;
// firebase end-----------------------------------

char authb[] = BLYNK_AUTH_TOKEN;

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = WIFI_SSID;
char pass[]= WIFI_PASSWORD;
// Attach virtual serial terminal to Virtual Pin V1
WidgetTerminal terminal(V1);
// blynk end--------------------------------------

/* pulse sensor start-----------------------------
int const PULSE_SENSOR_PIN = 0;        // Pulse Sensor PURPLE WIRE connected to ANALOG PIN 0
int LED13 = 2;                      //  The on-board Arduion LED TUKAR FROM 13 TO 2

int Signal;                // holds the incoming raw data. Signal value can range from 0-1024
int Threshold = 550;       // Determine which Signal to "count as a beat", and which to ingore.
// pulse sensor end-------------------------------*/

// fall detection start---------------------------
const int MPU_addr=0x68;  // I2C address of the MPU-6050
int16_t AcX,AcY,AcZ,Tmp,GyX,GyY,GyZ;
float ax=0, ay=0, az=0, gx=0, gy=0, gz=0;
boolean fall = false; //stores if a fall has occurred
boolean trigger1=false; //stores if first trigger (lower threshold) has occurred
boolean trigger2=false; //stores if second trigger (upper threshold) has occurred
boolean trigger3=false; //stores if third trigger (orientation change) has occurred
//byte trigger1count=0; //stores the counts past since trigger 1 was set true
//byte trigger2count=0; //stores the counts past since trigger 2 was set true
int trigger3count=0; //stores the counts past since trigger 3 was set true
double angleChange=0;
// fall detection end-------------------------------
BlynkTimer timer;
void setup()
{
  
  // mpu 6050 setup------------------------------------
  Serial.begin(115200);
  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);  // PWR_MGMT_1 register
  Wire.write(0);     // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);
  // mpu 6050 setup-------------------------------------
  
  // firebase setup-------------------------------------
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);
  pinMode(LED_BUILTIN,OUTPUT);
  // Update database path
  databasePath = "/readings";
  
  // firebase setup-------------------------------------
  // blynk setup----------------------------------------
  Blynk.begin(authb, ssid, pass, IPAddress(128,199,144,129), 8080);
  timer.setInterval (100L, sendSensor);
  // blynk setup----------------------------------------
  tone(14, 494, 500);//beep to indicate device is ready
}

void loop()
{
  // fall detection loop----------------------------------------
  mpu_read();
  ax = (AcX)/16384.00;  //ax = (AcX-2200)/16384.00;ax = (AcX+1674)/16384.00;
  ay = (AcY)/16384.00; // ay = (AcY-16400)/16384.00;ay = (AcY-1679)/16384.00;
  az = (AcZ)/16384.00; //az = (AcZ-5800)/16384.00;az = (AcZ-1518)/16384.00;
  gx = (GyX)/131.07; //gx = (GyX+270)/131.07;
  gy = (GyY)/131.07;  //gy = (GyY-351)/131.07;
  gz = (GyZ)/131.07;  //gz = (GyZ+136)/131.07;
  Serial.println(ax);
  Serial.println(ay);
  Serial.println(az);
  // calculating Amplitute vactor for 3 axis
  double Raw_Amp = pow(pow(ax,2)+pow(ay,2)+pow(az,2),0.5);
  float Amp = Raw_Amp * 10;  // Mulitiplied by 10 bcz values are between 0 to 1
  Serial.println(Amp);
   /*Blynk.virtualWrite(V7, Amp);
  Signal = analogRead(PULSE_SENSOR_PIN); // Read the sensor value

  Serial.println(Signal);                // Send the signal value to serial plotter
  //Blynk.virtualWrite(V11, Signal);

 (if(Signal > Threshold){                // If the signal is above threshold, turn on the LED
    digitalWrite(LED_BUILTIN,LOW);
  } else {
    digitalWrite(LED_BUILTIN,HIGH);     // Else turn off the LED
  }
  delay(10);*/
  
  if (Amp>=3 && trigger2==false)   //if (Amp>=2 && trigger2==false)
  { //if AM breaks lower threshold (0.4g) --------isn't it supposed to be terbalik...laju dulu lpatu baru lower threshold
    trigger1=true;
    Serial.println("TRIGGER 1 ACTIVATED");
  }
  
  if (trigger1==true)
  {  
    if (Amp>=12)  //if (Amp>=12)
    { //if AM breaks upper threshold (3g) original value is 12
      trigger2=true;
      Serial.println("TRIGGER 2 ACTIVATED");
      trigger1=false; 
    }
  }
  if (trigger2==true)
  {
    angleChange = pow(pow(gx,2)+pow(gy,2)+pow(gz,2),0.5); 
    Serial.println(angleChange);
    if (angleChange>=30 && angleChange<=400) //if (angleChange>=30 && angleChange<=400)
    { //if orientation changes by between 80-100 degrees
      trigger3=true; trigger2=false; 
      Serial.println(angleChange);
      Serial.println("TRIGGER 3 ACTIVATED");
    }
  }
  if (trigger3==true)
  {
    trigger3count++;
    if (trigger3count>=6) 
      angleChange = pow(pow(gx,2)+pow(gy,2)+pow(gz,2),0.5);
      Serial.println(angleChange); 
      if ((angleChange>=0) && (angleChange<=10))
      { //if orientation changes remains between 0-10 degrees after 5 seconds
        fall=true; trigger3=false; trigger3count=0;
        Serial.println(angleChange);
      }
      else if (trigger3count<6)
      { //user regained normal orientation
        trigger3=false; trigger3count=0;
        Serial.println("TRIGGER 3 DEACTIVATED");
      }
  }
  if (fall==true)
  { //in event of a fall detection
    Serial.println("FALL DETECTED");
    //Firebase.RTDB.setFloat(&fbdo, F("/fall"), fall) ? "ok" : fbdo.errorReason().c_str();
    tone(14, 494, 500);
    Blynk.logEvent("FALL ALERT",String("The wearer have fallen"));
    json.set(F("/fall"), String(fall));
    fall=false;
    delay(10000);
  }
  /*if (trigger2count>=6)
  { //allow 0.5s for orientation change
    trigger2=false; trigger2count=0;
    Serial.println("TRIGGER 2 DEACTIVATED");
  }
  if (trigger1count>=6)
  { //allow 0.5s for AM to break upper threshold
    trigger1=false; trigger1count=0;
    Serial.println("TRIGGER 1 DEACTIVATED");
  }*/
  //fall detection loop------------------------------------------------

  //firebase loop for storing data-------------------------------------
  parentPath= databasePath + "/" + String(count);

  json.set(F("/acceleration/x"), String(ax));
  json.set(F("/acceleration/y"), String(ay));
  json.set(F("/acceleration/z"), String(az));
  json.set(F("/orientation/x"), String(gx));
  json.set(F("/orientation/y"), String(gy));
  json.set(F("/orientation/z"), String(gz));
  Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
  //firebase loop------------------------------------------------------

  //blynk show the virtual pin output----------------------------------
  Blynk.run();
  timer.run();
  //blynk show the virtual pin output----------------------------------
  count++;
  delay(10);
}

void mpu_read()
{
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr,14,true);  // request a total of 14 registers// Wire.requestFrom(MPU_addr,14,true);  // request a total of 14 registers
  AcX=Wire.read()<<8|Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)    
  AcY=Wire.read()<<8|Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  AcZ=Wire.read()<<8|Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  Tmp=Wire.read()<<8|Wire.read();  // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  GyX=Wire.read()<<8|Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  GyY=Wire.read()<<8|Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  GyZ=Wire.read()<<8|Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
}

void sendSensor()
{
  
  float wx = gx;
  float wy = gy;
  float wz = gz;
  //float heartbeat = Signal-465;
  //Blynk.virtualWrite(V11, heartbeat);
  Blynk.virtualWrite(V8, wx);
  Blynk.virtualWrite(V9, wy);
  Blynk.virtualWrite(V10, wz);
}
