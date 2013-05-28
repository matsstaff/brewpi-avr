/*
 * Copyright 2012 BrewPi/Elco Jacobs.
 *
 * This file is part of BrewPi.
 * 
 * BrewPi is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later v7ersion.
 * 
 * BrewPi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with BrewPi.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Brewpi.h"
#include <limits.h>

#include "Display.h"
#include "DisplayLcd.h"
#include "Menu.h"
#include "TempControl.h"
#include "TemperatureFormats.h"
#include "Pins.h"

static const int LCD_FLAG_DISPLAY_ROOM = 0x01;

uint8_t LcdDisplay::stateOnDisplay;
bool LcdDisplay::flags;
SpiLcd LcdDisplay::lcd;

// Constant strings used multiple times
static const char STR_Beer_[] PROGMEM = "Beer ";
static const char STR_Fridge_[] PROGMEM = "Fridge ";
static const char STR_Const_[] PROGMEM = "Const.";
static const char STR_for[] PROGMEM = "for";
static const char STR_Waiting_to_[] PROGMEM = "Waiting to ";
static const char STR_Cooling_[] PROGMEM = "Cooling ";
static const char STR_Heating_[] PROGMEM = "Heating ";
static const char STR_min_time[] PROGMEM = "min time";

void LcdDisplay::init(void){
	stateOnDisplay = 0xFF; // set to unknown state to force update
	flags = 0;
	lcd.init(); // initialize LCD
	lcd.begin(20, 4);
	lcd.clear();
}

//print all temperatures on the LCD
void LcdDisplay::printAllTemperatures(void){
	bool displayRoom = ((ticks.seconds()&0x08)==0) && !BREWPI_SIMULATE;
	if (displayRoom ^ ((flags & LCD_FLAG_DISPLAY_ROOM)!=0)) {	// transition
		if (!tempControl.ambientSensor->isConnected())	{
			displayRoom = tempControl.ambientSensor->init()!=DEVICE_DISCONNECTED && displayRoom;
		}
		flags = displayRoom ? flags | LCD_FLAG_DISPLAY_ROOM : flags & ~LCD_FLAG_DISPLAY_ROOM;
		printStationaryText();
	}
	
	printBeerTemp();
	printBeerSet();
	printFridgeTemp();
	printFridgeSet();
}

void LcdDisplay::printBeerTemp(void){
	lcd.setCursor(6,1);
	if (flags & LCD_FLAG_DISPLAY_ROOM) {
		if (tempControl.ambientSensor->isConnected())
			printTemperature(tempControl.ambientSensor->read());			
		else
			printUndefinedTemperature();
	}
	else {
		if(tempControl.beerSensor->isConnected())
			printTemperature(tempControl.getBeerTemp());		
		else
			printUndefinedTemperature();			
	}
}

void LcdDisplay::printBeerSet(void){
	lcd.setCursor(12,1);
	fixed7_9 beerSet = tempControl.getBeerSetting();	
	if((flags & LCD_FLAG_DISPLAY_ROOM) || (beerSet == INT_MIN)){ // beer setting is not active
		printUndefinedTemperature();
	}
	else{
		printTemperature(beerSet);	
	}		
}

void LcdDisplay::printFridgeTemp(void){	
	lcd.setCursor(6,2);
	if(tempControl.fridgeSensor->isConnected()){
		printTemperature(tempControl.getFridgeTemp());
	}
	else{
		printUndefinedTemperature();
	}
}

void LcdDisplay::printFridgeSet(void){	
	lcd.setCursor(12,2);
	fixed7_9 fridgeSet = tempControl.getFridgeSetting();	
	if(fridgeSet == INT_MIN){ // beer setting is not active
		printUndefinedTemperature();
	}
	else{
		printTemperature(fridgeSet);
	}		
}

void LcdDisplay::printTemperature(fixed7_9 temp){
	char tempString[9];
	tempToString(tempString, temp, 1 , 9);
	for(uint8_t i = 0; i<(5-strlen(tempString));i++){
		lcd.write(' ');
	}
	lcd.print(tempString);
}

void LcdDisplay::printUndefinedTemperature(void){
	lcd.print_P(PSTR(" --.-"));
}

//print the stationary text on the lcd.
void LcdDisplay::printStationaryText(void){
	lcd.setCursor(0,0);
	lcd.print_P(PSTR("Mode"));
	lcd.setCursor(0,1);
		
	lcd.print_P((flags & LCD_FLAG_DISPLAY_ROOM) ?  PSTR("Room") : STR_Beer_);
		
	lcd.setCursor(0,2);
	lcd.print_P(STR_Fridge_); 
		
	lcd.setCursor(18,1);
	printDegreeUnit();
	lcd.setCursor(18,2);
	printDegreeUnit();
}

//print degree sign + C
void LcdDisplay::printDegreeUnit(void){
	lcd.write(0b11011111);
	lcd.write(tempControl.cc.tempFormat);	
}

// print mode on the right location on the first line, after "Mode   "
void LcdDisplay::printMode(void){
	lcd.setCursor(7,0);
	switch(tempControl.getMode()){
		case MODE_FRIDGE_CONSTANT:
			lcd.print_P(STR_Fridge_);
			lcd.print_P(STR_Const_);
			break;
		case MODE_BEER_CONSTANT:
			lcd.print_P(STR_Beer_);
			lcd.print_P(STR_Const_);
			break;
		case MODE_BEER_PROFILE:
			lcd.print_P(STR_Beer_);
			lcd.print_P(PSTR("Profile"));
			break;
		case MODE_OFF:
			lcd.print_P(PSTR("Off"));
			break;
		case MODE_TEST:
			lcd.print_P(PSTR("** Testing **"));
			break;
		default:
			lcd.print_P(PSTR("Invalid mode"));
			break;
	}
	lcd.writeSpacesToRestOfLine();
}

// print the current state on the last line of the lcd
void LcdDisplay::printState(void){
	uint16_t time;
	uint8_t state = tempControl.getState();
	uint8_t counterPrintPos=0;
	if(state != stateOnDisplay){ //only print static text when state has changed
		lcd.setCursor(0,3);
		// Reprint state and clear rest of the line
		switch (tempControl.getState()){
			case IDLE:
				lcd.print_P(PSTR("Idle "));
				lcd.print_P(STR_for);
				break;
			case WAITING_TO_COOL:
				lcd.print_P(STR_Waiting_to_);
				lcd.print_P(PSTR("cool"));
				break;
			case WAITING_TO_HEAT:
				lcd.print_P(STR_Waiting_to_);
				lcd.print_P(PSTR("heat"));
				break;
			case WAITING_FOR_PEAK_DETECT:
				lcd.print_P(PSTR("Awaiting peak detect"));
				break;
			case COOLING:
				lcd.print_P(STR_Cooling_);
				lcd.print_P(STR_for);
				break;
			case HEATING:
				lcd.print_P(STR_Heating_);
				lcd.print_P(STR_for);
				break;
			case COOLING_MIN_TIME:
				lcd.print_P(STR_Cooling_);
				lcd.print_P(STR_min_time);
				break;
			case HEATING_MIN_TIME:
				lcd.print_P(STR_Heating_);
				lcd.print_P(STR_min_time);
				break;
			case DOOR_OPEN:
				lcd.print_P(PSTR("Door open"));
				break;
			case STATE_OFF:
				lcd.print_P(PSTR("Temp. control OFF"));
				break;
			default:
				lcd.print_P(PSTR("Unknown status!"));
				break;
		}
		stateOnDisplay = state;
		// erase rest of the line by writing spaces
		lcd.writeSpacesToRestOfLine();
	}
	uint16_t idleTime = tempControl.timeSinceIdle();
	if(state==IDLE){
		counterPrintPos = 9;
		time = 	min(tempControl.timeSinceCooling(), tempControl.timeSinceHeating());
	}
	else if(state==COOLING || state==HEATING){
		counterPrintPos = 12;
		time = idleTime;
	}
	else if(state==COOLING_MIN_TIME || state==HEATING_MIN_TIME){
		counterPrintPos = 17;
		time = idleTime;
	}
	else if(state == WAITING_TO_COOL || state == WAITING_TO_HEAT){
		counterPrintPos = 16;
		time = tempControl.getWaitTime();
	}
	else{
		return;
	}	
	lcd.setCursor(counterPrintPos, 3);
	lcd.print(time);
}
