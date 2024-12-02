// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/midi/usb_midi_output_stream.h"

#include "base/logging.h"
#include "media/midi/message_util.h"
#include "media/midi/usb_midi_device.h"

namespace midi {

UsbMidiOutputStream::UsbMidiOutputStream(const UsbMidiJack& jack)
    : jack_(jack), pending_size_(0), is_sending_sysex_(false) {}

void UsbMidiOutputStream::Send(const std::vector<uint8_t>& data) {
  DCHECK_LT(jack_.cable_number, 16u);

  std::vector<uint8_t> data_to_send;
  size_t current = 0;
  size_t size = GetSize(data);
  while (current < size) {
    uint8_t first_byte = Get(data, current);
    if (first_byte == kSysExByte || is_sending_sysex_) {
      // System Exclusive messages
      if (!PushSysExMessage(data, &current, &data_to_send))
        break;
    } else if ((first_byte & kSysMessageBitMask) == kSysMessageBitPattern) {
      if (first_byte & 0x08) {
        // System Real-Time messages
        PushSysRTMessage(data, &current, &data_to_send);
      } else {
        // System Common messages
        if (!PushSysCommonMessage(data, &current, &data_to_send))
          break;
      }
    } else if (first_byte & 0x80) {
      if (!PushChannelMessage(data, &current, &data_to_send))
        break;
    } else {
      // Unknown messages
      DVLOG(1) << "Unknown byte: " << static_cast<unsigned int>(first_byte);
      ++current;
    }
  }

  if (data_to_send.size() > 0)
    jack_.device->Send(jack_.endpoint_number(), data_to_send);

  DCHECK_LE(current, size);
  DCHECK_LE(size - current, kPacketContentSize);
  // Note that this can be a self-copying and the iteration order is important.
  for (size_t i = current; i < size; ++i)
    pending_data_[i - current] = Get(data, i);
  pending_size_ = size - current;
}

size_t UsbMidiOutputStream::GetSize(const std::vector<uint8_t>& data) const {
  return data.size() + pending_size_;
}

uint8_t UsbMidiOutputStream::Get(const std::vector<uint8_t>& data,
                                 size_t index) const {
  DCHECK_LT(index, GetSize(data));
  if (index < pending_size_)
    return pending_data_[index];
  return data[index - pending_size_];
}

bool UsbMidiOutputStream::PushSysExMessage(const std::vector<uint8_t>& data,
                                           size_t* current,
                                           std::vector<uint8_t>* data_to_send) {
  size_t index = *current;
  size_t message_size = 0;
  const size_t kMessageSizeMax = 3;
  uint8_t message[kMessageSizeMax] = {};

  while (index < GetSize(data)) {
    if (message_size == kMessageSizeMax) {
      // We can't find the end-of-message mark in the three bytes.
      *current = index;
      data_to_send->push_back((jack_.cable_number << 4) | 0x4);
      data_to_send->insert(data_to_send->end(), message,
                           message + std::size(message));
      is_sending_sysex_ = true;
      return true;
    }
    uint8_t byte = Get(data, index);
    if ((byte & kSysRTMessageBitMask) == kSysRTMessageBitPattern) {
      // System Real-Time messages interleaved in a SysEx message
      PushSysRTMessage(data, &index, data_to_send);
      continue;
    }

    message[message_size] = byte;
    ++message_size;
    if (byte == kEndOfSysExByte) {
      uint8_t code_index = static_cast<uint8_t>(message_size) + 0x4;
      DCHECK(code_index == 0x5 || code_index == 0x6 || code_index == 0x7);
      data_to_send->push_back((jack_.cable_number << 4) | code_index);
      data_to_send->insert(data_to_send->end(), message,
                           message + std::size(message));
      *current = index + 1;
      is_sending_sysex_ = false;
      return true;
    }
    ++index;
  }
  return false;
}

bool UsbMidiOutputStream::PushSysCommonMessage(
    const std::vector<uint8_t>& data,
    size_t* current,
    std::vector<uint8_t>* data_to_send) {
  size_t index = *current;
  uint8_t first_byte = Get(data, index);
  DCHECK_LE(0xf1, first_byte);
  DCHECK_LE(first_byte, 0xf7);
  DCHECK_EQ(0xf0, first_byte & 0xf8);
  // There are only 6 message types (0xf1 - 0xf7), so the table size is 8.
  const size_t message_size_table[8] = {
    0, 2, 3, 2, 1, 1, 1, 0,
  };
  size_t message_size = message_size_table[first_byte & 0x07];
  DCHECK_NE(0u, message_size);
  DCHECK_LE(message_size, 3u);

  if (GetSize(data) < index + message_size) {
    // The message is incomplete.
    return false;
  }

  uint8_t code_index =
      message_size == 1 ? 0x5 : static_cast<uint8_t>(message_size);
  data_to_send->push_back((jack_.cable_number << 4) | code_index);
  for (size_t i = index; i < index + 3; ++i)
    data_to_send->push_back(i < index + message_size ? Get(data, i) : 0);
  *current += message_size;
  return true;
}

void UsbMidiOutputStream::PushSysRTMessage(const std::vector<uint8_t>& data,
                                           size_t* current,
                                           std::vector<uint8_t>* data_to_send) {
  size_t index = *current;
  uint8_t first_byte = Get(data, index);
  DCHECK_LE(0xf8, first_byte);
  DCHECK_LE(first_byte, 0xff);

  data_to_send->push_back((jack_.cable_number << 4) | 0x5);
  data_to_send->push_back(first_byte);
  data_to_send->push_back(0);
  data_to_send->push_back(0);
  *current += 1;
}

bool UsbMidiOutputStream::PushChannelMessage(
    const std::vector<uint8_t>& data,
    size_t* current,
    std::vector<uint8_t>* data_to_send) {
  size_t index = *current;
  uint8_t first_byte = Get(data, index);

  DCHECK_LE(0x80, (first_byte & 0xf0));
  DCHECK_LE((first_byte & 0xf0), 0xe0);
  // There are only 7 message types (0x8-0xe in the higher four bits), so the
  // table size is 8.
  const size_t message_size_table[8] = {
    3, 3, 3, 3, 2, 3, 3, 0,
  };
  uint8_t code_index = first_byte >> 4;
  DCHECK_LE(0x08, code_index);
  DCHECK_LE(code_index, 0x0e);
  size_t message_size = message_size_table[code_index & 0x7];
  DCHECK_NE(0u, message_size);
  DCHECK_LE(message_size, 3u);

  if (GetSize(data) < index + message_size) {
    // The message is incomplete.
    return false;
  }

  data_to_send->push_back((jack_.cable_number << 4) | code_index);
  for (size_t i = index; i < index + 3; ++i)
    data_to_send->push_back(i < index + message_size ? Get(data, i) : 0);
  *current += message_size;
  return true;
}

}  // namespace midi
