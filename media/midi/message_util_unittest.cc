// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/midi/message_util.h"

#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace midi {
namespace {

const uint8_t kGMOn[] = {0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7};
const uint8_t kGSOn[] = {
    0xf0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7f, 0x00, 0x41, 0xf7,
};
const uint8_t kNoteOn[] = {0x90, 0x3c, 0x7f};
const uint8_t kNoteOnWithRunningStatus[] = {
    0x90, 0x3c, 0x7f, 0x3c, 0x7f, 0x3c, 0x7f,
};
const uint8_t kNoteOnWithRealTimeClock[] = {
    0x90, 0xf8, 0x3c, 0x7f, 0x90, 0xf8, 0x3c, 0xf8, 0x7f, 0xf8,
};
const uint8_t kGMOnWithRealTimeClock[] = {
    0xf0, 0xf8, 0x7e, 0x7f, 0x09, 0x01, 0xf8, 0xf7,
};
const uint8_t kSystemCommonMessageReserved1[] = {0xf4};
const uint8_t kSystemCommonMessageReserved2[] = {0xf5};
const uint8_t kSystemCommonMessageTuneRequest[] = {0xf6};
const uint8_t kChannelPressure[] = {0xd0, 0x01};
const uint8_t kChannelPressureWithRunningStatus[] = {0xd0, 0x01, 0x01, 0x01};
const uint8_t kTimingClock[] = {0xf8};
const uint8_t kBrokenData1[] = {0x90};
const uint8_t kBrokenData2[] = {0xf7};
const uint8_t kBrokenData3[] = {0xf2, 0x00};
const uint8_t kDataByte0[] = {0x00};

template <typename T, size_t N>
const std::vector<T> AsVector(const T (&data)[N]) {
  std::vector<T> buffer;
  buffer.insert(buffer.end(), data, data + N);
  return buffer;
}

template <typename T, size_t N>
void PushToVector(const T (&data)[N], std::vector<T>* buffer) {
  buffer->insert(buffer->end(), data, data + N);
}

TEST(MidiMessageUtilTest, GetMessageLength) {
  // Check basic functionarity
  EXPECT_EQ(std::size(kNoteOn), GetMessageLength(kNoteOn[0]));
  EXPECT_EQ(std::size(kChannelPressure), GetMessageLength(kChannelPressure[0]));
  EXPECT_EQ(std::size(kTimingClock), GetMessageLength(kTimingClock[0]));
  EXPECT_EQ(std::size(kSystemCommonMessageTuneRequest),
            GetMessageLength(kSystemCommonMessageTuneRequest[0]));

  // SysEx message should be mapped to 0-length
  EXPECT_EQ(0u, GetMessageLength(kGMOn[0]));

  // Any reserved message should be mapped to 0-length
  EXPECT_EQ(0u, GetMessageLength(kSystemCommonMessageReserved1[0]));
  EXPECT_EQ(0u, GetMessageLength(kSystemCommonMessageReserved2[0]));

  // Any data byte should be mapped to 0-length
  EXPECT_EQ(0u, GetMessageLength(kGMOn[1]));
  EXPECT_EQ(0u, GetMessageLength(kNoteOn[1]));
  EXPECT_EQ(0u, GetMessageLength(kChannelPressure[1]));
}

TEST(MidiMessageUtilTest, IsValidWebMIDIData) {
  // Test single event scenario
  EXPECT_TRUE(IsValidWebMIDIData(AsVector(kGMOn)));
  EXPECT_TRUE(IsValidWebMIDIData(AsVector(kGSOn)));
  EXPECT_TRUE(IsValidWebMIDIData(AsVector(kNoteOn)));
  EXPECT_TRUE(IsValidWebMIDIData(AsVector(kChannelPressure)));
  EXPECT_TRUE(IsValidWebMIDIData(AsVector(kTimingClock)));
  EXPECT_FALSE(IsValidWebMIDIData(AsVector(kBrokenData1)));
  EXPECT_FALSE(IsValidWebMIDIData(AsVector(kBrokenData2)));
  EXPECT_FALSE(IsValidWebMIDIData(AsVector(kBrokenData3)));
  EXPECT_FALSE(IsValidWebMIDIData(AsVector(kDataByte0)));

  // MIDI running status should be disallowed
  EXPECT_FALSE(IsValidWebMIDIData(AsVector(kNoteOnWithRunningStatus)));
  EXPECT_FALSE(IsValidWebMIDIData(AsVector(kChannelPressureWithRunningStatus)));

  // Multiple messages are allowed as long as each of them is complete.
  {
    std::vector<uint8_t> buffer;
    PushToVector(kGMOn, &buffer);
    PushToVector(kNoteOn, &buffer);
    PushToVector(kGSOn, &buffer);
    PushToVector(kTimingClock, &buffer);
    PushToVector(kNoteOn, &buffer);
    EXPECT_TRUE(IsValidWebMIDIData(buffer));
    PushToVector(kBrokenData1, &buffer);
    EXPECT_FALSE(IsValidWebMIDIData(buffer));
  }

  // MIDI realtime message can be placed at any position.
  EXPECT_TRUE(IsValidWebMIDIData(AsVector(kNoteOnWithRealTimeClock)));
  EXPECT_TRUE(IsValidWebMIDIData(AsVector(kGMOnWithRealTimeClock)));
}

}  // namespace
}  // namespace midi
