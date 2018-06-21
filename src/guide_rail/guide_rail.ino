#include <Adafruit_TCS34725.h>
#include <Wire.h>
#include "rgb_lcd.h"
#include "Ultrasonic.h"

#define DEVICE_NAME           "GuideRail\n"
#define SOFTWARE_VERSION      "V1.0.0\n"

rgb_lcd lcd;
Ultrasonic ultrasonic(10);    // Arduino D10 for Ultrasonic Ranger 

enum pick_mode_e {
  RED_MODE = 0,
  GREEN_MODE,
  YELLOW_MODE,
};

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_700MS, TCS34725_GAIN_1X);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);             // <! debug uart
  Serial.print( DEVICE_NAME );
  Serial.print( SOFTWARE_VERSION );
  Serial2.begin(115200);            // <! uarm uart

  pinMode(5,INPUT_PULLUP);           // <! test key

  //initiate stepper driver
  pinMode(A2,OUTPUT);
  pinMode(2,OUTPUT);
  pinMode(3,OUTPUT);
  pinMode(4,OUTPUT);
  pinMode(A6,OUTPUT); //STEP
  pinMode(A7,OUTPUT); //DIR
  
  digitalWrite(A2,HIGH);
  digitalWrite(2,HIGH);//MS3
  digitalWrite(3,LOW);//MS2
  digitalWrite(4,LOW);//MS1
  digitalWrite(A6,LOW);
  digitalWrite(A7,LOW);
  
  Serial2.write("M2400 S0\n");                  // <! set the uArm work mode
  //Serial2.write("M2122 V1\n");                 // <! report when finish the movemnet
  Serial2.write("G2202 N3 V90\n");              // <! set effect-end servo angle
  Serial2.write("G0 X180 Y0 Z160 F2000000\n");  // <! move to start position
                   
  if( !tcs.begin() ){                            // <! init color sensor
    Serial.print( "color sensor error!" );
  }
  
  lcd.begin(16, 2);                             // <!  init 1602 LCD 
  lcd.setRGB(0, 0, 255);
  lcd.print("  guide rail  ");
  lcd.setCursor(0, 1);
  lcd.print("  count:");
}

void uarm_pick_up(){                           // <! control uArm to pick up a block
  Serial2.write("G0 X250 Y100 Z100 F2000000\n");
  delay(100);
  Serial2.write("G0 X250 Y100 Z100 F2000000\n");
  delay(100);
  Serial2.write("G0 X250 Y100 Z-25 F2000000\n");
  delay(2000);
  Serial2.write("M2231 V1\n");
  delay(1000);
  Serial2.write("G0 X180 Y0 Z160 F2000000\n");
  delay(1000);
}

void uarm_pick_down(enum pick_mode_e mode){   // <! control uArm to pick dowm the block and according to the color choosing position 

  switch(mode){
    case RED_MODE: 
            Serial2.write("G0 X200 Y-80 Z100 F2000000\n");
            delay(100);     
            Serial2.write("G0 X200 Y-80 Z-42 F2000000\n");
            delay(100); 
      break;
    case GREEN_MODE: 
          Serial2.write("G0 X200 Y0 Z100 F2000000\n");
          delay(100);
          Serial2.write("G0 X200 Y0 Z-42 F2000000\n");
          delay(100);
      break;
    case YELLOW_MODE:   
          Serial2.write("G0 X200 Y80 Z100 F2000000\n");
          delay(100);
          Serial2.write("G0 X200 Y80 Z-42 F2000000\n");
          delay(100);
      break;
  }
  delay(3000);
  Serial2.write("M2231 V0\n");
  delay(1000);
  Serial2.write("G0 X180 Y0 Z160 F2000000\n");
  delay(2000);
}


void delay_us(uint32_t us) {
  while (us) {
    if (us < 10) { 
      _delay_us(1);
      us--;
    } else if (us < 100) {
      _delay_us(10);
      us -= 10;
    } else if (us < 1000) {
      _delay_us(100);
      us -= 100;
    } else {
      _delay_ms(1);
      us -= 1000;
    }
  }
}

void rail_move_point(uint32_t step){        // <! control the guide rail move to the point
  static uint32_t current_step = 0;
  int long offset = step - current_step;
  if( offset == 0 ){ return; }
  current_step = step;
  if( offset > 0 ){
    digitalWrite(A7,LOW);
  }else{
    digitalWrite(A7,HIGH);
  }
  static bool level_state = false;
  offset = abs(offset);
  for( uint32_t i = 0; i < offset; i++ ){
    if( !level_state ){
      PORTF |= (1<<6);
      level_state = true;
    }else{
      PORTF &= (~(1<<6));
      level_state = false;
    }
    if( i>4000 && i<offset-4000){
      _delay_us(30); 
    }else{
      _delay_us(50); 
    }
  }
  delay(2000);
}

bool is_red(uint16_t r, uint16_t g, uint16_t b){        // <! is the red block?
  if( (r<800) || (r>1500) ){
    return false;
  }
  if( (g<200) || (g>1000) ){
    return false;
  }
  if( (b<200) || (b>1000) ){
    return false;
  }
  return true;  
}

bool is_green(uint16_t r, uint16_t g, uint16_t b){    // <! is the green block?
  if( (r<500) || (r>1500) ){
    return false;
  }
  if( (g<1000) || (g>1800) ){
    return false;
  }
  if( (b<200) || (b>1500) ){
    return false;
  }
  return true;
}

bool is_yellow(uint16_t r, uint16_t g, uint16_t b){   // <! is the yellow block?
  if( (r<1300) || (r>2500) ){
    return false;
  }
  if( (g<1300) || (g>2500) ){
    return false;
  }
  if( (b<500) || (b>1500) ){
    return false;
  }
  return true;
}


void loop() {
  uint16_t r, g, b, c;
  static int pick_cnt = 0;
  static bool detect_flag = false;
  tcs.getRawData(&r, &g, &b, &c);       // <! read the color sensor value

  if(  is_red(r, g, b) ){
    delay(50);
    if(  is_red(r, g, b) ){
      Serial.print( "red mode\r\n" );
      rail_move_point(0); 
      delay(1000);
      if( ultrasonic.MeasureInCentimeters() < 10 ){
        pick_cnt++;
      }     
      uarm_pick_up(); 
      rail_move_point(300000);
      uarm_pick_down(RED_MODE);
      lcd.setCursor(8, 1);
      lcd.print(pick_cnt);      
      rail_move_point(225000);
    }   
  }else if( is_green(r, g, b) ){
    delay(50);
    if(  is_green(r, g, b) ){
      Serial.print( "green mode\r\n" );
      rail_move_point(0); 
      delay(1000);
      if( ultrasonic.MeasureInCentimeters() < 10 ){
        pick_cnt++;
      }            
      uarm_pick_up(); 
      rail_move_point(300000);
      uarm_pick_down(GREEN_MODE);
      lcd.setCursor(8, 1);
      lcd.print(pick_cnt);     
      rail_move_point(150000); 
    }
  }else if( is_yellow(r, g, b) ){
    delay(50);
    if(  is_yellow(r, g, b) ){
      Serial.print( "yellow mode\r\n" );
      rail_move_point(0); 
      if( ultrasonic.MeasureInCentimeters() < 10 ){
         pick_cnt++; 
      }      
      uarm_pick_up(); 
      rail_move_point(300000);
      uarm_pick_down(YELLOW_MODE);
      lcd.setCursor(8, 1);
      lcd.print(pick_cnt);
      rail_move_point(75000); 
    }
  }
  
  if( digitalRead(5)==LOW ){                // <! clear the count
    pick_cnt = 0;
    lcd.setCursor(8, 1);
    lcd.print("        ");
  }
}
/*
  Serial.print("R: "); Serial.print(r, DEC); Serial.print(" ");
  Serial.print("G: "); Serial.print(g, DEC); Serial.print(" ");
  Serial.print("B: "); Serial.print(b, DEC); Serial.print("\r\n");
  */
