#include "BluetoothSerial.h"
BluetoothSerial SerialBT;

// -------- Motor Pins --------
#define PWMA  18
#define AIN1  19
#define AIN2  21
#define PWMB  5
#define BIN1  17
#define BIN2  16
#define STBY  23

// -------- Ultrasonic --------
#define TRIG_LEFT    32
#define ECHO_LEFT    33
#define TRIG_CENTER  25
#define ECHO_CENTER  26
#define TRIG_RIGHT   27
#define ECHO_RIGHT   14

// -------- Tunable Parameters --------
float Kp = 3.4;
float Ki = 0.0;
float Kd = 1.6;

int BASE_SPEED = 185;
int TURN_SPEED = 95;
int MAX_SPEED  = 220;

int TURN_DELAY = 280;
int LOOP_DELAY = 15;

#define FRONT_BLOCKED 22
float CENTER_OFFSET = -2.2;

float error, prevError, correction;
bool robotEnabled = true;

// ---------------- Motor ----------------
void setMotor(int L, int R) {
  if (L >= 0) { digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW); }
  else { digitalWrite(AIN1, LOW); digitalWrite(AIN2, HIGH); L = -L; }

  if (R >= 0) { digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW); }
  else { digitalWrite(BIN1, LOW); digitalWrite(BIN2, HIGH); R = -R; }

  analogWrite(PWMA, constrain(L,0,255));
  analogWrite(PWMB, constrain(R,0,255));
}

// ---------------- Ultrasonic ----------------
float readDistance(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH, 30000);
  if(duration==0) return 150;
  return duration*0.034/2;
}

// ---------------- Bluetooth ----------------
void handleBluetooth() {
  if (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim(); cmd.toUpperCase();
    if (cmd=="STOP"){ robotEnabled=false; setMotor(0,0); return; }
    if (cmd=="RUN"){ robotEnabled=true; return; }

    char t = cmd.charAt(0);
    float v = cmd.substring(1).toFloat();
    if(t=='P')Kp=v; if(t=='D')Kd=v; if(t=='B')BASE_SPEED=v;
    if(t=='T')TURN_SPEED=v; if(t=='R')TURN_DELAY=v; if(t=='L')LOOP_DELAY=v;
  }
}

// ---------------- Setup ----------------
void setup(){
  SerialBT.begin("MazeBot_Tuner");
  pinMode(AIN1,OUTPUT); pinMode(AIN2,OUTPUT);
  pinMode(BIN1,OUTPUT); pinMode(BIN2,OUTPUT);
  pinMode(PWMA,OUTPUT); pinMode(PWMB,OUTPUT);
  pinMode(STBY,OUTPUT);
  pinMode(TRIG_LEFT,OUTPUT); pinMode(ECHO_LEFT,INPUT);
  pinMode(TRIG_CENTER,OUTPUT); pinMode(ECHO_CENTER,INPUT);
  pinMode(TRIG_RIGHT,OUTPUT); pinMode(ECHO_RIGHT,INPUT);
  digitalWrite(STBY,HIGH);
}

// ---------------- Loop ----------------
void loop(){

  handleBluetooth();
  if(!robotEnabled){ setMotor(0,0); delay(50); return; }

  float L = readDistance(TRIG_LEFT,ECHO_LEFT);
  float F = readDistance(TRIG_CENTER,ECHO_CENTER);
  float R = readDistance(TRIG_RIGHT,ECHO_RIGHT);

  // -------- SMART TURN DETECTION --------
  if(F < FRONT_BLOCKED && L < 30 && R < 30){
    setMotor(TURN_SPEED, TURN_SPEED); delay(100);

    if(L>R) setMotor(-TURN_SPEED,TURN_SPEED);
    else setMotor(TURN_SPEED,-TURN_SPEED);

    if(F<12) delay(TURN_DELAY*2);
    else delay(TURN_DELAY);
    return;
  }

  // -------- WALL LOSS DETECTION --------
  if(L > 80 && R < 40) L = 40;   // left wall gone
  if(R > 80 && L < 40) R = 40;   // right wall gone

  // -------- DYNAMIC CENTERING --------
  float center = (L+R)/2.0;
  error = (L-center)+CENTER_OFFSET;

  float derivative = error-prevError;
  correction = (Kp*error)+(Kd*derivative);
  prevError=error;

  // -------- ADAPTIVE SPEED --------
  int speed = BASE_SPEED;
  if(F>90 && abs(error)<2) speed = MAX_SPEED;  // turbo
  if(abs(error)>7) speed -= 35;                // curves
  if(F<40) speed -= 40;                        // approach turn

  int leftSpeed  = speed - correction;
  int rightSpeed = speed + correction;

  leftSpeed  = constrain(leftSpeed,0,MAX_SPEED);
  rightSpeed = constrain(rightSpeed,0,MAX_SPEED);

  setMotor(leftSpeed,rightSpeed);
  delay(LOOP_DELAY);
}
