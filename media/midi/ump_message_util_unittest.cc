// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/ump_message_util.h"

#include <vector>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace midi {

std::vector<uint8_t> ToVector(base::span<const uint8_t> span) {
  return std::vector<uint8_t>(span.begin(), span.end());
}

// Universal MIDI Packet (UMP) General Structure:
// UMP consists of 1 to 4 32-bit words. The first word (Word 0) always contains
// the following header in its most significant bits:
// - Bits 28-31 (4 bits): Message Type (MT)
// - Bits 24-27 (4 bits): Group
//
// Therefore, in these tests:
// - We shift the Message Type by 28 (e.g., `mt << 28`) to place it at bits
// 28-31.
// - We shift the Group by 24 (e.g., `group << 24`) to place it at bits 24-27.

TEST(UmpMessageUtilTest, GetUmpMessageType) {
  // Test that GetUmpMessageType correctly extracts the 4-bit Message Type (MT)
  // from the most significant bits (bits 28-31) of the UMP Word 0.
  EXPECT_EQ(0x0u, GetUmpMessageType(0x0u << 28));  // MT=0 (Utility)
  EXPECT_EQ(0x1u, GetUmpMessageType(0x1u << 28));  // MT=1 (System Common / RT)
  EXPECT_EQ(0x2u, GetUmpMessageType(0x2u << 28));  // MT=2 (MIDI 1.0 Voice)
  EXPECT_EQ(0x3u, GetUmpMessageType(0x3u << 28));  // MT=3 (SysEx7)
  EXPECT_EQ(0xFu, GetUmpMessageType(0xFu << 28));  // MT=15 (Stream)
}

TEST(UmpMessageUtilTest, GetUmpGroup) {
  // Test that GetUmpGroup correctly extracts the 4-bit Group (0-15)
  // from bits 24-27 of the UMP Word 0.
  EXPECT_EQ(0x0u, GetUmpGroup(0x0u << 24));  // Group 0
  EXPECT_EQ(0x1u, GetUmpGroup(0x1u << 24));  // Group 1
  EXPECT_EQ(0x7u, GetUmpGroup(0x7u << 24));  // Group 7
  EXPECT_EQ(0xFu, GetUmpGroup(0xFu << 24));  // Group 15
}

TEST(UmpMessageUtilTest, GetUmpLengthInWords) {
  // Test that GetUmpLengthInWords returns the correct UMP size in 32-bit words
  // based on the Message Type (MT) in Word 0.

  // MT=0 (Utility) -> 1 word
  EXPECT_EQ(1u, GetUmpLengthInWords(0x0u << 28));
  // MT=1 (System Common / Real-Time) -> 1 word
  EXPECT_EQ(1u, GetUmpLengthInWords(0x1u << 28));
  // MT=2 (MIDI 1.0 Channel Voice) -> 1 word
  EXPECT_EQ(1u, GetUmpLengthInWords(0x2u << 28));
  // MT=3 (SysEx7) -> 2 words
  EXPECT_EQ(2u, GetUmpLengthInWords(0x3u << 28));
  // MT=4 (MIDI 2.0 Channel Voice) -> 2 words
  EXPECT_EQ(2u, GetUmpLengthInWords(0x4u << 28));
  // MT=5 (SysEx8) -> 4 words
  EXPECT_EQ(4u, GetUmpLengthInWords(0x5u << 28));
  // MT=D (Flex Data) -> 4 words
  EXPECT_EQ(4u, GetUmpLengthInWords(0xDu << 28));
  // MT=F (UMP Stream) -> 4 words
  EXPECT_EQ(4u, GetUmpLengthInWords(0xFu << 28));

  // MT=6 (Reserved / Undefined) -> Should return 0 words (invalid)
  EXPECT_EQ(0u, GetUmpLengthInWords(0x6u << 28));
}

TEST(UmpMessageUtilTest, ParseMidiMessages) {
  // 1. Channel Voice (Note On, Note Off)
  // Input: Note On (0x90, 0x3C, 0x50) followed by Note Off (0x80, 0x3C, 0x00).
  // Expectation: Two distinct 3-byte messages.
  std::vector<uint8_t> data1 = {0x90, 0x3C, 0x50, 0x80, 0x3C, 0x00};
  auto msgs1 = ParseMidiMessages(data1);
  ASSERT_EQ(2u, msgs1.size());
  EXPECT_FALSE(msgs1[0].is_sysex);
  EXPECT_EQ((std::vector<uint8_t>{0x90, 0x3C, 0x50}),
            ToVector(msgs1[0].GetData()));
  EXPECT_FALSE(msgs1[1].is_sysex);
  EXPECT_EQ((std::vector<uint8_t>{0x80, 0x3C, 0x00}),
            ToVector(msgs1[1].GetData()));

  // 2. SysEx (Complete: 0xF0 ... 0xF7)
  // Input: SysEx Start (0xF0), data bytes (0x01, 0x02, 0x03), SysEx End (0xF7).
  // Expectation: One complete 5-byte SysEx message.
  std::vector<uint8_t> data2 = {0xF0, 0x01, 0x02, 0x03, 0xF7};
  auto msgs2 = ParseMidiMessages(data2);
  ASSERT_EQ(1u, msgs2.size());
  EXPECT_TRUE(msgs2[0].is_sysex);
  EXPECT_EQ((std::vector<uint8_t>{0xF0, 0x01, 0x02, 0x03, 0xF7}),
            ToVector(msgs2[0].GetData()));

  // 3. SysEx (Not terminated but ends at buffer limit: 0xF0 ...)
  // Input: SysEx Start (0xF0), data bytes (0x01, 0x02, 0x03). No 0xF7.
  // Expectation: One 4-byte SysEx message.
  std::vector<uint8_t> data3 = {0xF0, 0x01, 0x02, 0x03};
  auto msgs3 = ParseMidiMessages(data3);
  ASSERT_EQ(1u, msgs3.size());
  EXPECT_TRUE(msgs3[0].is_sysex);
  EXPECT_EQ((std::vector<uint8_t>{0xF0, 0x01, 0x02, 0x03}),
            ToVector(msgs3[0].GetData()));

  // 4. Real-Time messages interleaved with Note On.
  // Input: Timing Clock (0xF8) [1 byte], Note On (0x90, 0x3C, 0x50) [3 bytes],
  // Active Sensing (0xFE) [1 byte].
  // Expectation: Three distinct messages.
  std::vector<uint8_t> data5 = {0xF8, 0x90, 0x3C, 0x50, 0xFE};
  auto msgs5 = ParseMidiMessages(data5);
  ASSERT_EQ(3u, msgs5.size());
  EXPECT_FALSE(msgs5[0].is_sysex);
  EXPECT_EQ((std::vector<uint8_t>{0xF8}), ToVector(msgs5[0].GetData()));
  EXPECT_FALSE(msgs5[1].is_sysex);
  EXPECT_EQ((std::vector<uint8_t>{0x90, 0x3C, 0x50}),
            ToVector(msgs5[1].GetData()));
  EXPECT_FALSE(msgs5[2].is_sysex);
  EXPECT_EQ((std::vector<uint8_t>{0xFE}), ToVector(msgs5[2].GetData()));

  // 5. Interleaved Real-Time message in SysEx.
  // Input: SysEx Start (0xF0), data (0x01, 0x02), Timing Clock (0xF8), data
  // (0x03), SysEx End (0xF7). Expectation: Two distinct messages:
  // - Timing Clock (0xF8)
  // - SysEx (0xF0, 0x01, 0x02, 0x03, 0xF7) (with F8 removed)
  std::vector<uint8_t> data6 = {0xF0, 0x01, 0x02, 0xF8, 0x03, 0xF7};
  auto msgs6 = ParseMidiMessages(data6);
  ASSERT_EQ(2u, msgs6.size());
  EXPECT_FALSE(msgs6[0].is_sysex);
  EXPECT_EQ((std::vector<uint8_t>{0xF8}), ToVector(msgs6[0].GetData()));
  EXPECT_TRUE(msgs6[1].is_sysex);
  EXPECT_EQ((std::vector<uint8_t>{0xF0, 0x01, 0x02, 0x03, 0xF7}),
            ToVector(msgs6[1].GetData()));

  // 6. Interleaved Real-Time message in Channel Voice Message.
  // Input: Note On (0x90, 0x3C), Active Sensing (0xFE), Note On velocity
  // (0x50). Expectation: Two distinct messages:
  // - Active Sensing (0xFE)
  // - Note On (0x90, 0x3C, 0x50) (with FE removed)
  std::vector<uint8_t> data7 = {0x90, 0x3C, 0xFE, 0x50};
  auto msgs7 = ParseMidiMessages(data7);
  ASSERT_EQ(2u, msgs7.size());
  EXPECT_FALSE(msgs7[0].is_sysex);
  EXPECT_EQ((std::vector<uint8_t>{0xFE}), ToVector(msgs7[0].GetData()));
  EXPECT_FALSE(msgs7[1].is_sysex);
  EXPECT_EQ((std::vector<uint8_t>{0x90, 0x3C, 0x50}),
            ToVector(msgs7[1].GetData()));
}

TEST(UmpMessageUtilTest, DispatchMidiFromUmpWords) {
  // Test that DispatchMidiFromUmpWords correctly parses a stream of UMP words
  // and dispatches them as legacy MIDI 1.0 bytes to the callback.
  // This covers all MIDI 1.0 message lengths (1, 2, and 3 bytes) for both
  // Channel Voice (MT=2) and System Common/Real-Time (MT=1) messages, as well
  // as all SysEx7 packet status types (Complete, Start, Continue, End).
  std::vector<uint32_t> ump_words = {
      // 1. Note On (MT=2, Group=0, Note=3C, Velocity=50) - 3 bytes
      // UMP: [MT=2 (4 bits) | Group=0 (4 bits) | Status=90 (8 bits) |
      //       Data1=3C (8 bits) | Data2=50 (8 bits)]
      // Expectation: Dispatches 3 bytes [0x90, 0x3C, 0x50].
      (2u << 28) | (0u << 24) | (0x90u << 16) | (0x3Cu << 8) | 0x50u,

      // 2. Program Change (MT=2, Group=0, Program=12) - 2 bytes
      // UMP: [MT=2 (4 bits) | Group=0 (4 bits) | Status=C0 (8 bits) |
      //       Data1=12 (8 bits) | Data2=00 (8 bits)]
      // Expectation: Dispatches 2 bytes [0xC0, 0x12].
      (2u << 28) | (0u << 24) | (0xC0u << 16) | (0x12u << 8),

      // 3. Complete SysEx (MT=3, Group=0, Status=0 (Complete), Len=3),
      //    payload: 01 02 03
      // Word 0: [MT=3 (4 bits) | Group=0 (4 bits) | Status=0 (4 bits) |
      //          Len=3 (4 bits) | Byte1=01 (8 bits) | Byte2=02 (8 bits)]
      // Word 1: [Byte3=03 (8 bits) | remaining bytes zero]
      // Expectation: Dispatches 5 bytes [0xF0, 0x01, 0x02, 0x03, 0xF7].
      (3u << 28) | (0u << 24) | (0u << 20) | (3u << 16) | (0x01u << 8) | 0x02u,
      (0x03u << 24),

      // 4. SysEx Start (MT=3, Group=0, Status=1 (Start), Len=6),
      //    payload: 01 02 03 04 05 06
      // Word 0: [MT=3 (4 bits) | Group=0 (4 bits) | Status=1 (4 bits) |
      //          Len=6 (4 bits) | Byte1=01 (8 bits) | Byte2=02 (8 bits)]
      // Word 1: [Byte3=03 (8 bits) | Byte4=04 (8 bits) | Byte5=05 (8 bits) |
      //          Byte6=06 (8 bits)]
      // Expectation: Dispatches 7 bytes [0xF0, 0x01, ..., 0x06].
      (3u << 28) | (0u << 24) | (1u << 20) | (6u << 16) | (0x01u << 8) | 0x02u,
      (0x03u << 24) | (0x04u << 16) | (0x05u << 8) | 0x06u,

      // 5. SysEx Continue (MT=3, Group=0, Status=2 (Continue), Len=6),
      //    payload: 07 08 09 0A 0B 0C
      // Word 0: [MT=3 (4 bits) | Group=0 (4 bits) | Status=2 (4 bits) |
      //          Len=6 (4 bits) | Byte1=07 (8 bits) | Byte2=08 (8 bits)]
      // Word 1: [Byte3=09 (8 bits) | Byte4=0A (8 bits) | Byte5=0B (8 bits) |
      //          Byte6=0C (8 bits)]
      // Expectation: Dispatches 6 bytes [0x07, ..., 0x0C].
      (3u << 28) | (0u << 24) | (2u << 20) | (6u << 16) | (0x07u << 8) | 0x08u,
      (0x09u << 24) | (0x0Au << 16) | (0x0Bu << 8) | 0x0Cu,

      // 6. SysEx End (MT=3, Group=0, Status=3 (End), Len=1), payload: 0D
      // Word 0: [MT=3 (4 bits) | Group=0 (4 bits) | Status=3 (4 bits) |
      //          Len=1 (4 bits) | Byte1=0D (8 bits) | Byte2=00 (8 bits)]
      // Word 1: all bytes zero
      // Expectation: Dispatches 2 bytes [0x0D, 0xF7].
      (3u << 28) | (0u << 24) | (3u << 20) | (1u << 16) | (0x0Du << 8), 0u,

      // 7. MIDI Time Code Quarter Frame (MT=1, Group=0, Status=F1, Data1=15) -
      // 2 bytes
      // UMP: [MT=1 (4 bits) | Group=0 (4 bits) | Status=F1 (8 bits) |
      //       Data1=15 (8 bits) | Data2=00 (8 bits)]
      // Expectation: Dispatches 2 bytes [0xF1, 0x15].
      (1u << 28) | (0u << 24) | (0xF1u << 16) | (0x15u << 8),

      // 8. Song Position Pointer (MT=1, Group=0, Status=F2, Data1=24, Data2=48)
      // - 3 bytes
      // UMP: [MT=1 (4 bits) | Group=0 (4 bits) | Status=F2 (8 bits) |
      //       Data1=24 (8 bits) | Data2=48 (8 bits)]
      // Expectation: Dispatches 3 bytes [0xF2, 0x24, 0x48].
      (1u << 28) | (0u << 24) | (0xF2u << 16) | (0x24u << 8) | 0x48u,

      // 9. Tune Request (MT=1, Group=0, Status=F6) - 1 byte
      // UMP: [MT=1 (4 bits) | Group=0 (4 bits) | Status=F6 (8 bits) |
      //       Data1=00 (8 bits) | Data2=00 (8 bits)]
      // Expectation: Dispatches 1 byte [0xF6].
      (1u << 28) | (0u << 24) | (0xF6u << 16),

      // 10. Active Sensing (MT=1, Group=0, Status=FE) - 1 byte
      // UMP: [MT=1 (4 bits) | Group=0 (4 bits) | Status=FE (8 bits) |
      //       Data1=00 (8 bits) | Data2=00 (8 bits)]
      // Expectation: Dispatches 1 byte [0xFE].
      (1u << 28) | (0u << 24) | (0xFEu << 16)};

  base::TimeTicks timestamp = base::TimeTicks::Now();
  std::vector<std::vector<uint8_t>> received_messages;
  std::vector<base::TimeTicks> received_timestamps;

  DispatchMidiFromUmpWords(
      ump_words, timestamp,
      [&](base::span<const uint8_t> data, base::TimeTicks ts) {
        received_messages.emplace_back(data.begin(), data.end());
        received_timestamps.push_back(ts);
      });

  ASSERT_EQ(10u, received_messages.size());
  // 1. Note On (3 bytes)
  EXPECT_EQ((std::vector<uint8_t>{0x90, 0x3C, 0x50}), received_messages[0]);
  // 2. Program Change (2 bytes)
  EXPECT_EQ((std::vector<uint8_t>{0xC0, 0x12}), received_messages[1]);
  // 3. Complete SysEx (5 bytes)
  EXPECT_EQ((std::vector<uint8_t>{0xF0, 0x01, 0x02, 0x03, 0xF7}),
            received_messages[2]);
  // 4. SysEx Start chunk
  EXPECT_EQ((std::vector<uint8_t>{0xF0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06}),
            received_messages[3]);
  // 5. SysEx Continue chunk (6 bytes)
  EXPECT_EQ((std::vector<uint8_t>{0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C}),
            received_messages[4]);
  // 6. SysEx End chunk
  EXPECT_EQ((std::vector<uint8_t>{0x0D, 0xF7}), received_messages[5]);
  // 7. MIDI Time Code (2 bytes)
  EXPECT_EQ((std::vector<uint8_t>{0xF1, 0x15}), received_messages[6]);
  // 8. Song Position Pointer (3 bytes)
  EXPECT_EQ((std::vector<uint8_t>{0xF2, 0x24, 0x48}), received_messages[7]);
  // 9. Tune Request (1 byte)
  EXPECT_EQ((std::vector<uint8_t>{0xF6}), received_messages[8]);
  // 10. Active Sensing (1 byte)
  EXPECT_EQ((std::vector<uint8_t>{0xFE}), received_messages[9]);

  for (auto ts : received_timestamps) {
    EXPECT_EQ(timestamp, ts);
  }
}

TEST(UmpMessageUtilTest, TranslateMidiToUmpWords) {
  // Test that TranslateMidiToUmpWords correctly translates a stream of legacy
  // MIDI 1.0 bytes into UMP words.
  // This covers all MIDI 1.0 message lengths (1, 2, and 3 bytes) for both
  // Channel Voice and System Common/Real-Time messages, as well as SysEx
  // packet splitting (Complete, Start, Continue, End).
  std::vector<uint8_t> legacy_data = {
      // 1. Note On (0x90, 0x3C, 0x50) [3 bytes]
      0x90, 0x3C, 0x50,

      // 2. Program Change (0xC0, 0x12) [2 bytes]
      0xC0, 0x12,

      // 3. Complete SysEx (5 legacy bytes, 3 payload bytes)
      // Expectation: 1 UMP packet (2 words), Status=0 (Complete), Len=3
      0xF0, 0x01, 0x02, 0x03, 0xF7,

      // 4. Long SysEx (14 legacy bytes, 12 payload bytes)
      // Expectation: 2 UMP packets (4 words):
      // - Packet 1: Status=1 (Start), Len=6 [01 02 03 04 05 06]
      // - Packet 2: Status=3 (End), Len=6 [07 08 09 0A 0B 0C]
      0xF0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
      0x0C, 0xF7,

      // 5. MIDI Time Code (0xF1, 0x15) [2 bytes]
      0xF1, 0x15,

      // 6. Song Position Pointer (0xF2, 0x24, 0x48) [3 bytes]
      0xF2, 0x24, 0x48,

      // 7. Tune Request (0xF6) [1 byte]
      0xF6,

      // 8. Interleaved Real-Time message in SysEx (6 bytes)
      // F0 01 02 F8 03 F7
      // Expectation:
      // - Timing Clock (0xF8) -> 1 UMP packet (1 word)
      // - Complete SysEx payload (01 02 03) -> 1 UMP packet (2 words)
      0xF0, 0x01, 0x02, 0xF8, 0x03, 0xF7,

      // 9. Interleaved Real-Time message in Channel Voice Message (4 bytes)
      // Note On (0x90, 0x3C), Active Sensing (0xFE), Note On velocity (0x50)
      // Expectation:
      // - Active Sensing (0xFE) -> 1 UMP packet (1 word)
      // - Note On (0x90, 0x3C, 0x50) -> 1 UMP packet (1 word)
      0x90, 0x3C, 0xFE, 0x50};

  std::vector<uint32_t> ump_words;
  TranslateMidiToUmpWords(legacy_data, 0, ump_words);

  ASSERT_EQ(16u, ump_words.size());  // 1 + 1 + 2 + 4 + 1 + 1 + 1 + (1 + 2) + (1
                                     // + 1) = 16 words

  size_t w = 0;
  // 1. Note On
  EXPECT_EQ((2u << 28) | (0u << 24) | (0x90u << 16) | (0x3Cu << 8) | 0x50u,
            ump_words[w++]);

  // 2. Program Change
  EXPECT_EQ((2u << 28) | (0u << 24) | (0xC0u << 16) | (0x12u << 8),
            ump_words[w++]);

  // 3. Complete SysEx (Status=0, Len=3)
  EXPECT_EQ(
      (3u << 28) | (0u << 24) | (0u << 20) | (3u << 16) | (0x01u << 8) | 0x02u,
      ump_words[w++]);
  EXPECT_EQ((0x03u << 24), ump_words[w++]);

  // 4. Long SysEx - Start (Status=1, Len=6)
  EXPECT_EQ(
      (3u << 28) | (0u << 24) | (1u << 20) | (6u << 16) | (0x01u << 8) | 0x02u,
      ump_words[w++]);
  EXPECT_EQ((0x03u << 24) | (0x04u << 16) | (0x05u << 8) | 0x06u,
            ump_words[w++]);

  // 4. Long SysEx - End (Status=3, Len=6)
  EXPECT_EQ(
      (3u << 28) | (0u << 24) | (3u << 20) | (6u << 16) | (0x07u << 8) | 0x08u,
      ump_words[w++]);
  EXPECT_EQ((0x09u << 24) | (0x0Au << 16) | (0x0Bu << 8) | 0x0Cu,
            ump_words[w++]);

  // 5. MIDI Time Code
  EXPECT_EQ((1u << 28) | (0u << 24) | (0xF1u << 16) | (0x15u << 8),
            ump_words[w++]);

  // 6. Song Position Pointer
  EXPECT_EQ((1u << 28) | (0u << 24) | (0xF2u << 16) | (0x24u << 8) | 0x48u,
            ump_words[w++]);

  // 7. Tune Request
  EXPECT_EQ((1u << 28) | (0u << 24) | (0xF6u << 16), ump_words[w++]);

  // 8. Interleaved Timing Clock (F8)
  EXPECT_EQ((1u << 28) | (0u << 24) | (0xF8u << 16), ump_words[w++]);

  // 8. Interleaved SysEx payload (01 02 03)
  EXPECT_EQ(
      (3u << 28) | (0u << 24) | (0u << 20) | (3u << 16) | (0x01u << 8) | 0x02u,
      ump_words[w++]);
  EXPECT_EQ((0x03u << 24), ump_words[w++]);

  // 9. Interleaved Active Sensing (FE)
  EXPECT_EQ((1u << 28) | (0u << 24) | (0xFEu << 16), ump_words[w++]);

  // 9. Interleaved Note On
  EXPECT_EQ((2u << 28) | (0u << 24) | (0x90u << 16) | (0x3Cu << 8) | 0x50u,
            ump_words[w++]);
}

TEST(UmpMessageUtilTest, SysEx7PayloadBoundariesRoundTrip) {
  // Section 7.7 permits 0 to 6 payload bytes per SysEx7 UMP. Verify the empty
  // and full Complete forms, plus the first size that requires Start and End.
  const std::vector<std::vector<uint8_t>> legacy_messages = {
      {0xF0, 0xF7},
      {0xF0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0xF7},
      {0xF0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0xF7},
  };
  const std::vector<std::vector<uint32_t>> expected_ump_words = {
      {
          (3u << 28) | (0u << 20) | (0u << 16),
          0u,
      },
      {
          (3u << 28) | (0u << 20) | (6u << 16) | (0x01u << 8) | 0x02u,
          (0x03u << 24) | (0x04u << 16) | (0x05u << 8) | 0x06u,
      },
      {
          (3u << 28) | (1u << 20) | (6u << 16) | (0x01u << 8) | 0x02u,
          (0x03u << 24) | (0x04u << 16) | (0x05u << 8) | 0x06u,
          (3u << 28) | (3u << 20) | (1u << 16) | (0x07u << 8),
          0u,
      },
  };

  ASSERT_EQ(legacy_messages.size(), expected_ump_words.size());
  for (size_t i = 0; i < legacy_messages.size(); ++i) {
    std::vector<uint32_t> ump_words;
    TranslateMidiToUmpWords(legacy_messages[i], 0, ump_words);
    EXPECT_EQ(expected_ump_words[i], ump_words);

    std::vector<uint8_t> round_trip_message;
    DispatchMidiFromUmpWords(
        ump_words, base::TimeTicks(),
        [&](base::span<const uint8_t> data, base::TimeTicks) {
          round_trip_message.insert(round_trip_message.end(), data.begin(),
                                    data.end());
        });
    EXPECT_EQ(legacy_messages[i], round_trip_message);
  }
}

TEST(UmpMessageUtilTest, TranslateMidiToUmpWordsIncomplete) {
  // Test that TranslateMidiToUmpWords discards incomplete messages at the end
  // and only translates complete ones.
  std::vector<uint8_t> incomplete_data = {
      0x90, 0x3C, 0x50,  // Note On (complete, 3 bytes)
      0xC0,              // Program Change (incomplete, 1 byte)
      0xF0, 0x01         // SysEx (incomplete, no 0xF7)
  };
  std::vector<uint32_t> ump_words;
  TranslateMidiToUmpWords(incomplete_data, 0, ump_words);
  ASSERT_EQ(1u, ump_words.size());  // Only Note On should be translated
  EXPECT_EQ((2u << 28) | (0u << 24) | (0x90u << 16) | (0x3Cu << 8) | 0x50u,
            ump_words[0]);
}

void DispatchMidiFromUmpWordsDoesNotCrash(
    base::span<const uint32_t> ump_words) {
  auto callback = [](base::span<const uint8_t> data,
                     base::TimeTicks timestamp) {
    // Do nothing, just consume the parsed data.
  };
  DispatchMidiFromUmpWords(ump_words, base::TimeTicks(), callback);
}

FUZZ_TEST(UmpMessageUtilFuzzTest, DispatchMidiFromUmpWordsDoesNotCrash)
    .WithDomains(fuzztest::Arbitrary<std::vector<uint32_t>>());

void TranslateMidiToUmpWordsDoesNotCrash(base::span<const uint8_t> data,
                                         uint8_t group) {
  std::vector<uint32_t> translated_words;
  TranslateMidiToUmpWords(data, group, translated_words);
}

FUZZ_TEST(UmpMessageUtilFuzzTest, TranslateMidiToUmpWordsDoesNotCrash)
    .WithDomains(fuzztest::Arbitrary<std::vector<uint8_t>>(),
                 fuzztest::InRange<uint8_t>(0, 15));

}  // namespace midi
