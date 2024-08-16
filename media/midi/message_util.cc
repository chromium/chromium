// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/message_util.h"

#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"

namespace midi {

size_t GetMessageLength(uint8_t status_byte) {
  if (status_byte < 0x80)
    return 0;
  if (0x80 <= status_byte && status_byte <= 0xbf)
    return 3;
  if (0xc0 <= status_byte && status_byte <= 0xdf)
    return 2;
  if (0xe0 <= status_byte && status_byte <= 0xef)
    return 3;

  switch (status_byte) {
    case 0xf0:
      return 0;
    case 0xf1:
      return 2;
    case 0xf2:
      return 3;
    case 0xf3:
      return 2;
    case 0xf4:  // Reserved
    case 0xf5:  // Reserved
      return 0;
    case 0xf6:
      return 1;
    case 0xf7:
      return 0;
    case 0xf8:
    case 0xf9:
    case 0xfa:
    case 0xfb:
    case 0xfc:
    case 0xfd:
    case 0xfe:
    case 0xff:
      return 1;
  }
  NOTREACHED();
}

bool IsDataByte(uint8_t data) {
  return (data & 0x80) == 0;
}

bool IsSystemRealTimeMessage(uint8_t data) {
  return 0xf8 <= data;
}

bool IsSystemMessage(uint8_t data) {
  return 0xf0 <= data;
}

bool IsValidWebMIDIData(const std::vector<uint8_t>& data) {
  bool in_sysex = false;
  size_t sysex_start_offset = 0;
  size_t waiting_data_length = 0;
  for (size_t i = 0; i < data.size(); ++i) {
    const uint8_t current = data[i];
    if (IsSystemRealTimeMessage(current))
      continue;  // Real time message can be placed at any point.
    if (waiting_data_length > 0) {
      if (!IsDataByte(current))
        return false;  // Error: |current| should have been data byte.
      --waiting_data_length;
      continue;  // Found data byte as expected.
    }
    if (in_sysex) {
      if (data[i] == kEndOfSysExByte) {
        in_sysex = false;
        UMA_HISTOGRAM_COUNTS_1M("Media.Midi.SysExMessageSizeUpTo1MB",
                                static_cast<base::HistogramBase::Sample>(
                                    i - sysex_start_offset + 1));
      } else if (!IsDataByte(current)) {
        return false;  // Error: |current| should have been data byte.
      }
      continue;  // Found data byte as expected.
    }
    if (current == kSysExByte) {
      in_sysex = true;
      sysex_start_offset = i;
      continue;  // Found SysEX
    }
    waiting_data_length = GetMessageLength(current);
    if (waiting_data_length == 0)
      return false;  // Error: |current| should have been a valid status byte.
    --waiting_data_length;  // Found status byte
  }
  return waiting_data_length == 0 && !in_sysex;
}

}  // namespace midi
