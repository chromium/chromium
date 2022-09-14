// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MESSAGE_UTIL_H_
#define MEDIA_MIDI_MESSAGE_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "media/midi/midi_export.h"

namespace midi {

// Returns the length of a MIDI message in bytes. Never returns 4 or greater.
// Returns 0 if |status_byte| is:
// - not a valid status byte, namely data byte.
// - MIDI System Exclusive message.
// - End of System Exclusive message.
// - Reserved System Common Message (0xf4, 0xf5)
MIDI_EXPORT size_t GetMessageLength(uint8_t status_byte);

// Checks if the specified byte is a valid data byte.
MIDI_EXPORT bool IsDataByte(uint8_t data);

// Checks if the specified byte is a valid system real time message.
MIDI_EXPORT bool IsSystemRealTimeMessage(uint8_t data);

// Checks if the specified byte is a valid system message.
MIDI_EXPORT bool IsSystemMessage(uint8_t data);

// Checks if |data| fulfills the requirements of MidiOutput.send API that is
// defined in the Web MIDI spec.
// - |data| must be any number of complete MIDI messages (data abbreviation
//    called "running status" is disallowed).
// - 1-byte MIDI realtime messages can be placed at any position of |data|.
MIDI_EXPORT bool IsValidWebMIDIData(const std::vector<uint8_t>& data);

const uint8_t kSysExByte = 0xf0;
const uint8_t kEndOfSysExByte = 0xf7;

const uint8_t kSysMessageBitMask = 0xf0;
const uint8_t kSysMessageBitPattern = 0xf0;
const uint8_t kSysRTMessageBitMask = 0xf8;
const uint8_t kSysRTMessageBitPattern = 0xf8;

}  // namespace midi

#endif  // MEDIA_MIDI_MESSAGE_UTIL_H_
