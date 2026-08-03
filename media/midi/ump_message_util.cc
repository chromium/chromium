// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/ump_message_util.h"

#include <algorithm>

#include "base/containers/span.h"
#include "base/functional/function_ref.h"
#include "base/time/time.h"

namespace midi {

namespace {

// Universal MIDI Packet (UMP) General Structure:
// UMP consists of 1 to 4 32-bit words. The first word (Word 0) always contains
// the following header in its most significant bits:
// - Bits 28-31 (4 bits): Message Type (MT)
// - Bits 24-27 (4 bits): Group

// UMP Message Type (MT) constants (4-bit)
constexpr uint8_t kUmpMsgTypeUtility = 0x0;
constexpr uint8_t kUmpMsgTypeSystem = 0x1;
constexpr uint8_t kUmpMsgTypeMidi1ChannelVoice = 0x2;
constexpr uint8_t kUmpMsgTypeSysEx7 = 0x3;
constexpr uint8_t kUmpMsgTypeMidi2ChannelVoice = 0x4;
constexpr uint8_t kUmpMsgTypeSysEx8 = 0x5;
constexpr uint8_t kUmpMsgTypeFlexData = 0xD;
constexpr uint8_t kUmpMsgTypeStream = 0xF;

// UMP SysEx7 Status constants (4-bit)
constexpr uint8_t kUmpSysExStatusComplete = 0x0;
constexpr uint8_t kUmpSysExStatusStart = 0x1;
constexpr uint8_t kUmpSysExStatusContinue = 0x2;
constexpr uint8_t kUmpSysExStatusEnd = 0x3;

// Bit shifts and masks for UMP parsing/construction
constexpr uint32_t kUmpMessageTypeShift = 28;
constexpr uint32_t kUmpMessageTypeMask = 0x0F;
constexpr uint32_t kUmpGroupShift = 24;
constexpr uint32_t kUmpGroupMask = 0x0F;

// MT=1 (System), MT=2 (MIDI 1.0) shifts and masks
constexpr uint32_t kUmpStatusShift = 16;
constexpr uint32_t kUmpStatusMask = 0xFF;
constexpr uint32_t kUmpData1Shift = 8;
constexpr uint32_t kUmpData1Mask = 0xFF;
constexpr uint32_t kUmpData2Shift = 0;
constexpr uint32_t kUmpData2Mask = 0xFF;

// MT=3 (SysEx7) shifts and masks
constexpr uint32_t kUmpSysExStatusShift = 20;
constexpr uint32_t kUmpSysExStatusMask = 0x0F;
constexpr uint32_t kUmpSysExSizeShift = 16;
constexpr uint32_t kUmpSysExSizeMask = 0x0F;

// SysEx7 Word 1 byte shifts (all bytes are 8-bit)
constexpr uint32_t kUmpSysExByte3Shift = 24;
constexpr uint32_t kUmpSysExByte4Shift = 16;
constexpr uint32_t kUmpSysExByte5Shift = 8;
constexpr uint32_t kUmpSysExByte6Shift = 0;

}  // namespace

uint8_t GetUmpMessageType(uint32_t first_word) {
  return (first_word >> kUmpMessageTypeShift) & kUmpMessageTypeMask;
}

uint8_t GetUmpGroup(uint32_t first_word) {
  return (first_word >> kUmpGroupShift) & kUmpGroupMask;
}

size_t GetUmpLengthInWords(uint32_t first_word) {
  uint8_t message_type = GetUmpMessageType(first_word);
  switch (message_type) {
    case kUmpMsgTypeUtility:
    case kUmpMsgTypeSystem:
    case kUmpMsgTypeMidi1ChannelVoice:
      return 1;
    case kUmpMsgTypeSysEx7:
    case kUmpMsgTypeMidi2ChannelVoice:
      return 2;
    case kUmpMsgTypeSysEx8:
    case kUmpMsgTypeFlexData:
    case kUmpMsgTypeStream:
      return 4;
    default:
      // Other types are reserved or undefined.
      return 0;
  }
}

std::vector<MidiMessage> ParseMidiMessages(base::span<const uint8_t> data) {
  std::vector<MidiMessage> messages;
  size_t index = 0;

  bool in_sysex_message = false;
  size_t sysex_message_start = 0;
  bool sysex_message_has_realtime_message = false;
  std::vector<uint8_t> active_sysex_message_reconstructed;

  bool in_channel_message = false;
  size_t channel_message_start = 0;
  bool channel_message_has_realtime_message = false;
  std::vector<uint8_t> active_channel_message_reconstructed;
  size_t expected_channel_message_length = 0;

  while (index < data.size()) {
    uint8_t status = data[index];

    if (status < 0x80) {
      if (in_sysex_message) {
        if (sysex_message_has_realtime_message) {
          active_sysex_message_reconstructed.push_back(status);
        }
      } else if (in_channel_message) {
        if (channel_message_has_realtime_message) {
          active_channel_message_reconstructed.push_back(status);
        }
        size_t current_len = channel_message_has_realtime_message
                                 ? active_channel_message_reconstructed.size()
                                 : (index + 1 - channel_message_start);
        if (current_len == expected_channel_message_length) {
          if (channel_message_has_realtime_message) {
            messages.push_back(
                {/*is_sysex=*/false,
                 {},
                 std::move(active_channel_message_reconstructed)});
            active_channel_message_reconstructed.clear();
          } else {
            messages.push_back({/*is_sysex=*/false,
                                data.subspan(channel_message_start,
                                             expected_channel_message_length)});
          }
          in_channel_message = false;
        }
      }
      index++;
      continue;
    }

    if (status >= 0xF8) {
      // Real-Time Message (1 byte). Can interleave anywhere.
      messages.push_back({/*is_sysex=*/false, data.subspan(index, 1u)});
      if (in_sysex_message) {
        if (!sysex_message_has_realtime_message) {
          sysex_message_has_realtime_message = true;
          active_sysex_message_reconstructed.assign(
              data.begin() + sysex_message_start, data.begin() + index);
        }
      } else if (in_channel_message) {
        if (!channel_message_has_realtime_message) {
          channel_message_has_realtime_message = true;
          active_channel_message_reconstructed.assign(
              data.begin() + channel_message_start, data.begin() + index);
        }
      }
      index++;
    } else {
      // Non-Real-Time Status byte.
      if (status == 0xF0) {
        if (in_sysex_message) {
          // Unclosed SysEx. Flush current one (without F0).
          if (sysex_message_has_realtime_message) {
            messages.push_back({/*is_sysex=*/true,
                                {},
                                std::move(active_sysex_message_reconstructed)});
            active_sysex_message_reconstructed.clear();
          } else {
            messages.push_back(
                {/*is_sysex=*/true, data.subspan(sysex_message_start,
                                                 index - sysex_message_start)});
          }
        }
        if (in_channel_message) {
          active_channel_message_reconstructed.clear();
          in_channel_message = false;
        }
        in_sysex_message = true;
        sysex_message_start = index;
        sysex_message_has_realtime_message = false;
        index++;
      } else if (status == 0xF7) {
        if (in_sysex_message) {
          // Normal SysEx termination.
          if (sysex_message_has_realtime_message) {
            active_sysex_message_reconstructed.push_back(status);
            messages.push_back({/*is_sysex=*/true,
                                {},
                                std::move(active_sysex_message_reconstructed)});
            active_sysex_message_reconstructed.clear();
          } else {
            messages.push_back({/*is_sysex=*/true,
                                data.subspan(sysex_message_start,
                                             index + 1 - sysex_message_start)});
          }
          in_sysex_message = false;
        }
        if (in_channel_message) {
          active_channel_message_reconstructed.clear();
          in_channel_message = false;
        }
        index++;
      } else {
        // Interrupt/terminate active SysEx or Channel message
        if (in_sysex_message) {
          if (sysex_message_has_realtime_message) {
            messages.push_back({/*is_sysex=*/true,
                                {},
                                std::move(active_sysex_message_reconstructed)});
            active_sysex_message_reconstructed.clear();
          } else {
            messages.push_back(
                {/*is_sysex=*/true, data.subspan(sysex_message_start,
                                                 index - sysex_message_start)});
          }
          in_sysex_message = false;
        }
        if (in_channel_message) {
          active_channel_message_reconstructed.clear();
          in_channel_message = false;
        }
        // Start of Channel Voice or System Common
        size_t len = 0;
        uint8_t status_type = status & 0xF0;
        if (status_type == 0xC0 || status_type == 0xD0 || status == 0xF1 ||
            status == 0xF3) {
          len = 2;
        } else if ((status_type >= 0x80 && status_type <= 0xEF) ||
                   status == 0xF2) {
          len = 3;
        } else if (status == 0xF6) {
          len = 1;
        }

        if (len == 1) {
          messages.push_back({/*is_sysex=*/false, data.subspan(index, 1u)});
        } else if (len > 1) {
          in_channel_message = true;
          channel_message_start = index;
          channel_message_has_realtime_message = false;
          expected_channel_message_length = len;
        }
        index++;
      }
    }
  }

  if (in_sysex_message) {
    if (sysex_message_has_realtime_message) {
      if (!active_sysex_message_reconstructed.empty()) {
        messages.push_back({/*is_sysex=*/true,
                            {},
                            std::move(active_sysex_message_reconstructed)});
      }
    } else {
      messages.push_back(
          {/*is_sysex=*/true, data.subspan(sysex_message_start,
                                           data.size() - sysex_message_start)});
    }
  }

  return messages;
}

void DispatchMidiFromUmpWords(
    base::span<const uint32_t> ump_words,
    base::TimeTicks timestamp,
    base::FunctionRef<void(base::span<const uint8_t> data,
                           base::TimeTicks timestamp)> dispatch_helper) {
  size_t word_index = 0;
  while (word_index < ump_words.size()) {
    uint32_t word0 = ump_words[word_index];
    size_t ump_length = GetUmpLengthInWords(word0);
    if (ump_length == 0 || word_index + ump_length > ump_words.size()) {
      // Malformed or unsupported UMP, or incomplete UMP in the stream.
      break;
    }

    uint8_t message_type = GetUmpMessageType(word0);

    if (message_type == kUmpMsgTypeSystem) {
      // UMP Format for MT=1 (System Common / Real-Time):
      // - Word 0: [MT(4) | Group(4) | Status(8) | Data1(8) | Data2(8)]
      uint8_t status = (word0 >> kUmpStatusShift) & kUmpStatusMask;
      uint8_t data1 = (word0 >> kUmpData1Shift) & kUmpData1Mask;
      uint8_t data2 = (word0 >> kUmpData2Shift) & kUmpData2Mask;

      uint8_t message[3] = {status, data1, data2};
      size_t length = 0;
      if (status == 0xF1 || status == 0xF3) {
        length = 2;
      } else if (status == 0xF2) {
        length = 3;
      } else if (status == 0xF6 || (status >= 0xF8 && status <= 0xFF)) {
        length = 1;
      }

      if (length > 0) {
        dispatch_helper(base::span(message).first(length), timestamp);
      }
    } else if (message_type == kUmpMsgTypeMidi1ChannelVoice) {
      // UMP Format for MT=2 (MIDI 1.0 Channel Voice):
      // - Word 0: [MT(4) | Group(4) | Status(8) | Data1(8) | Data2(8)]
      uint8_t status = (word0 >> kUmpStatusShift) & kUmpStatusMask;
      uint8_t data1 = (word0 >> kUmpData1Shift) & kUmpData1Mask;
      uint8_t data2 = (word0 >> kUmpData2Shift) & kUmpData2Mask;

      uint8_t message[3] = {status, data1, data2};
      size_t length = 0;
      uint8_t status_type = status & 0xF0;
      if (status_type == 0xC0 || status_type == 0xD0) {
        length = 2;
      } else if (status_type >= 0x80 && status_type <= 0xEF) {
        length = 3;
      }

      if (length > 0) {
        dispatch_helper(base::span(message).first(length), timestamp);
      }
    } else if (message_type == kUmpMsgTypeSysEx7) {
      // UMP Format for MT=3 (SysEx7):
      // - Word 0: [MT(4) | Group(4) | Status(4) | NumBytes(4) | Byte1(8) |
      // Byte2(8)]
      // - Word 1: [Byte3(8) | Byte4(8) | Byte5(8) | Byte6(8)]
      //
      // Per "UMP Format and MIDI 2.0 Protocol", section 7.7, the UMP payload
      // excludes the MIDI 1.0 0xF0/0xF7 bracketing bytes. Reconstruct those
      // bytes from the UMP status when converting back to MIDI 1.0:
      // https://midi.org/?p=1381
      uint32_t word1 = ump_words[word_index + 1];
      uint8_t ump_status =
          (word0 >> kUmpSysExStatusShift) & kUmpSysExStatusMask;
      uint8_t number_of_bytes =
          (word0 >> kUmpSysExSizeShift) & kUmpSysExSizeMask;

      if (ump_status <= kUmpSysExStatusEnd && number_of_bytes <= 6) {
        uint8_t payload[6];
        payload[0] = (word0 >> kUmpData1Shift) & kUmpData1Mask;
        payload[1] = (word0 >> kUmpData2Shift) & kUmpData2Mask;
        payload[2] = (word1 >> kUmpSysExByte3Shift) & 0xFF;
        payload[3] = (word1 >> kUmpSysExByte4Shift) & 0xFF;
        payload[4] = (word1 >> kUmpSysExByte5Shift) & 0xFF;
        payload[5] = (word1 >> kUmpSysExByte6Shift) & 0xFF;

        uint8_t message[8];
        auto message_span = base::span(message);
        size_t message_length = 0;
        if (ump_status == kUmpSysExStatusComplete ||
            ump_status == kUmpSysExStatusStart) {
          message_span[message_length++] = 0xF0;
        }
        for (uint8_t byte : base::span(payload).first(number_of_bytes)) {
          message_span[message_length++] = byte;
        }
        if (ump_status == kUmpSysExStatusComplete ||
            ump_status == kUmpSysExStatusEnd) {
          message_span[message_length++] = 0xF7;
        }

        if (message_length > 0) {
          dispatch_helper(message_span.first(message_length), timestamp);
        }
      }
    }

    word_index += ump_length;
  }
}

void TranslateMidiToUmpWords(base::span<const uint8_t> data,
                             uint8_t group,
                             std::vector<uint32_t>& ump_words) {
  std::vector<MidiMessage> messages = ParseMidiMessages(data);
  size_t message_index = 0;

  while (message_index < messages.size()) {
    const auto& message = messages[message_index];
    base::span<const uint8_t> message_data = message.GetData();
    if (!message.is_sysex) {
      if (message_data.empty()) {
        message_index++;
        continue;
      }
      uint8_t status = message_data[0];
      uint32_t message_type =
          (status >= 0xF0) ? kUmpMsgTypeSystem : kUmpMsgTypeMidi1ChannelVoice;

      uint32_t word = (message_type << kUmpMessageTypeShift) |
                      ((group & kUmpGroupMask) << kUmpGroupShift) |
                      (status << kUmpStatusShift);

      if (message_data.size() >= 2) {
        word |= (message_data[1] << kUmpData1Shift);
      }
      if (message_data.size() >= 3) {
        word |= (message_data[2] << kUmpData2Shift);
      }

      ump_words.push_back(word);
      message_index++;
    } else {
      // Outbound Web MIDI data is validated before reaching this utility, so
      // every SysEx message normally has its MIDI 1.0 0xF0/0xF7 brackets.
      // Keep this check because the utility is also called directly by a
      // fuzzer.
      if (message_data.size() < 2 || message_data.front() != 0xF0 ||
          message_data.back() != 0xF7) {
        message_index++;
        continue;
      }

      // "UMP Format and MIDI 2.0 Protocol", section 7.7, requires SysEx7 UMPs
      // to omit the MIDI 1.0 bracketing bytes and carry only their payload:
      // https://midi.org/?p=1381
      message_data =
          message_data.subspan<1>().first(message_data.size() - size_t{2});
      size_t sysex_offset = 0;
      do {
        size_t chunk_length = std::min(message_data.size() - sysex_offset,
                                       static_cast<size_t>(6));
        uint8_t ump_status = 0;
        if (message_data.size() <= 6) {
          ump_status = kUmpSysExStatusComplete;
        } else if (sysex_offset == 0) {
          ump_status = kUmpSysExStatusStart;
        } else if (sysex_offset + chunk_length == message_data.size()) {
          ump_status = kUmpSysExStatusEnd;
        } else {
          ump_status = kUmpSysExStatusContinue;
        }

        uint32_t words[2] = {0, 0};
        words[0] =
            (kUmpMsgTypeSysEx7 << kUmpMessageTypeShift) |
            ((group & kUmpGroupMask) << kUmpGroupShift) |
            ((ump_status & kUmpSysExStatusMask) << kUmpSysExStatusShift) |
            ((chunk_length & kUmpSysExSizeMask) << kUmpSysExSizeShift);

        uint8_t bytes[6] = {0};
        auto bytes_span = base::span(bytes);
        auto sysex_chunk = message_data.subspan(sysex_offset, chunk_length);
        for (size_t i = 0; i < chunk_length; ++i) {
          bytes_span[i] = sysex_chunk[i];
        }

        words[0] |= (bytes_span[0] << kUmpData1Shift) |
                    (bytes_span[1] << kUmpData2Shift);
        words[1] |= (bytes_span[2] << kUmpSysExByte3Shift) |
                    (bytes_span[3] << kUmpSysExByte4Shift) |
                    (bytes_span[4] << kUmpSysExByte5Shift) |
                    (bytes_span[5] << kUmpSysExByte6Shift);

        ump_words.push_back(words[0]);
        ump_words.push_back(words[1]);
        sysex_offset += chunk_length;
      } while (sysex_offset < message_data.size());
      message_index++;
    }
  }
}

}  // namespace midi
