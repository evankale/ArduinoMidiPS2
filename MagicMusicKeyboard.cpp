/*
 * Copyright (c) 2016 Evan Kale
 * Email: EvanKale91@gmail.com
 * Website: www.ISeeDeadPixel.com
 *          www.evankale.blogspot.ca
 *
 * This file is part of ArduinoMidiPS2.
 *
 * ArduinoMidiPS2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "MagicMusicKeyboard.h"
#include "BTKeyCodes.h"
#include "KeyTable.h"

//PS2 keycodes:
//http://www.computer-engineering.org/ps2keyboard/scancodes3.html

//MIDI defines
#define NOTE_ON_CMD 0x90
#define NOTE_OFF_CMD 0x80
#define NOTE_VELOCITY 70

MagicMusicKeyboard::MagicMusicKeyboard(uint8_t clockPin, uint8_t dataPin, uint8_t noteOffset)
	:PS2Device(clockPin, dataPin)
{
	_isInitialized = false;
	_noteOffset = noteOffset;
}

MagicMusicKeyboard::~MagicMusicKeyboard()
{
}

bool MagicMusicKeyboard::init()
{
	bool resetStatus = initReset();
	if (!resetStatus)
		return false;

	delayMicroseconds(100);

	_isInitialized = true;
	return true;
}

bool MagicMusicKeyboard::initReset()
{
	bool resetStatus = write(0xff, true);  // Reset signal - 0xFF

	if (!resetStatus)
	{
		return false;
	}

	read(NULL, false);  // Acknowledge - 0xFA

	// Self-test result, 0xAA00 when OK, 0xFC00 on error
	uint8_t result = 0x00;
	read(&result, false);

	if (result != 0xAA)
	{
		return false;
	}

	write(0xF0, false);  //select scan code set
	read(NULL, false);  // Acknowledge - 0xFA
	write(0x03, false);  //scan code set 3
	read(NULL, false);  // Acknowledge - 0xFA

	write(0xf8, false);  //Set all keys Make/Break (disables typmatic repeat)
	read(NULL, false);  // Acknowledge - 0xFA

	return true;
}

bool MagicMusicKeyboard::update()
{
	if (!_isInitialized)
		return false;

	if (tryRead())
		return true;

	return false;
}

bool MagicMusicKeyboard::tryRead()
{
	uint8_t keyCode;
	uint8_t btCode = BTKC_NONE;
	bool pressed = false;

	if (read(&keyCode, true))
	{
		//Read the rest of the incoming bytes with blocking

		if (keyCode == 0x00)
		{
			//do nothing
			return false;
		}
		else if (keyCode == 0xF0) //a standard key released
		{
			read(&keyCode, false);
			btCode = KeyTable::getBTCode(keyCode, false);
			pressed = false;
		}
		else if (keyCode == 0xE0) //an extended key
		{
			read(&keyCode, false);

			if (keyCode == 0xF0) //an extended key released
			{
				read(&keyCode, false);

				//The one exception to this case is Print Screen
				// Make: E0,12,E0,7C
				// Break: E0,F0,7C,E0,F0,12
				// So hardcode the case where 7C is read
				if (keyCode == 0x7C)
				{
					keyCode = 0x12;
					//skip over remaining 3 bytes
					read(NULL, false);
					read(NULL, false);
					read(NULL, false);
				}

				btCode = KeyTable::getBTCode(keyCode, true);
				pressed = false;
			}

			//The one exception to this case is Print Screen
			// Make: E0,12,E0,7C
			// Break: E0,F0,7C,E0,F0,12
			else if (keyCode == 0x12)
			{
				//skip over remaining 2 bytes
				read(NULL, false);
				read(NULL, false);
			}

			else //an extended key pressed
			{
				btCode = KeyTable::getBTCode(keyCode, true);
				pressed = true;
			}
		}
		else if (keyCode == 0xE1)
		{
			//There's only 1 E1 extension in the table of scancodes (PAUSE key)
			// (there's no release code for this key either)
			btCode = BTKC_PAUSE;
			pressed = true;

			//skip over remaning 7 bytes
			read(NULL, false);
			read(NULL, false);
			read(NULL, false);
			read(NULL, false);
			read(NULL, false);
			read(NULL, false);
			read(NULL, false);
		}
		else
		{
			btCode = KeyTable::getBTCode(keyCode, false);
			pressed = true;
		}
		return processKey(btCode, pressed);
	}
	return false;
}

bool MagicMusicKeyboard::processKey(uint8_t btCode, bool isPressed)
{
	bool stateUpdated = false;

	if (!(1 <= btCode && btCode <= BTKC_BACKSPACE))
	{
		//do nothing
		return false;
	}

	else if (btCode == BTKC_BACKSPACE)
	{
		//reset key (backspace)
		if (isPressed)
		{
			midiAllNotesOff();
			return true;
		}
		else
		{
			return false;
		}
	}

	else
	{
		if (isPressed)
		{
			midiNoteOn(btCode + _noteOffset);
		}
		else
		{
			midiNoteOff(btCode + _noteOffset);
		}
		return true;
	}
}

void MagicMusicKeyboard::midiNoteOn(uint8_t note)
{
	Serial.write(NOTE_ON_CMD);
	Serial.write(note);
	Serial.write(NOTE_VELOCITY);
}

void MagicMusicKeyboard::midiNoteOff(uint8_t note)
{
	Serial.write(NOTE_OFF_CMD);
	Serial.write(note);
	Serial.write(NOTE_VELOCITY);
}

void MagicMusicKeyboard::midiAllNotesOff()
{
	for (uint8_t i = 0; i < 0x80; ++i)
	{
		midiNoteOff(i);
	}
}
