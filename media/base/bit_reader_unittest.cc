// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/bit_reader.h"

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static void SetBit(base::span<uint8_t> buf, size_t bit_pos) {
  size_t byte_pos = bit_pos / 8;
  bit_pos -= byte_pos * 8;
  buf[byte_pos] |= (1 << (7 - bit_pos));
}

TEST(BitReaderTest, NormalOperationTest) {
  uint8_t value8;
  uint64_t value64;
  // 0101 0101 1001 1001 repeats 4 times
  uint8_t buffer[] = {0x55, 0x99, 0x55, 0x99, 0x55, 0x99, 0x55, 0x99};
  BitReader reader1(buffer, 6);  // Initialize with 6 bytes only

  EXPECT_TRUE(reader1.ReadBits(1, &value8));
  EXPECT_EQ(value8, 0);
  EXPECT_TRUE(reader1.ReadBits(8, &value8));
  EXPECT_EQ(value8, 0xab);  // 1010 1011
  EXPECT_TRUE(reader1.ReadBits(7, &value64));
  EXPECT_TRUE(reader1.ReadBits(32, &value64));
  EXPECT_EQ(value64, 0x55995599u);
  EXPECT_FALSE(reader1.ReadBits(1, &value8));
  value8 = 0xff;
  EXPECT_TRUE(reader1.ReadBits(0, &value8));
  EXPECT_EQ(value8, 0);

  BitReader reader2(buffer);
  EXPECT_TRUE(reader2.ReadBits(64, &value64));
  EXPECT_EQ(value64, 0x5599559955995599ull);
  EXPECT_FALSE(reader2.ReadBits(1, &value8));
  EXPECT_TRUE(reader2.ReadBits(0, &value8));
}

TEST(BitReaderTest, ReadBeyondEndTest) {
  uint8_t value8;
  uint8_t buffer[] = {0x12};
  BitReader reader1(buffer);

  EXPECT_TRUE(reader1.ReadBits(4, &value8));
  EXPECT_FALSE(reader1.ReadBits(5, &value8));
  EXPECT_FALSE(reader1.ReadBits(1, &value8));
  EXPECT_TRUE(reader1.ReadBits(0, &value8));
}

TEST(BitReaderTest, SkipBitsTest) {
  uint8_t value8;
  uint8_t buffer[] = {0x0a, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  BitReader reader1(buffer);

  EXPECT_TRUE(reader1.SkipBits(2));
  EXPECT_TRUE(reader1.ReadBits(3, &value8));
  EXPECT_EQ(value8, 1);
  EXPECT_TRUE(reader1.SkipBits(11));
  EXPECT_TRUE(reader1.ReadBits(8, &value8));
  EXPECT_EQ(value8, 3);
  EXPECT_TRUE(reader1.SkipBits(76));
  EXPECT_TRUE(reader1.ReadBits(4, &value8));
  EXPECT_EQ(value8, 13);
  EXPECT_FALSE(reader1.SkipBits(100));
  EXPECT_TRUE(reader1.SkipBits(0));
  EXPECT_FALSE(reader1.SkipBits(1));
}

TEST(BitReaderTest, VariableSkipBitsTest) {
  uint8_t buffer[256] = {};

  // The test alternates between ReadBits and SkipBits.
  // The first number is the number of bits to read, the second one is the
  // number of bits to skip. The number of bits to read was arbitrarily chosen
  // while the number of bits to skip was chosen so as to cover from small skips
  // to large skips.
  const auto pattern_read_skip =
      std::to_array<std::tuple<const size_t, const size_t>>({
          {5, 17},
          {4, 34},
          {0, 44},
          {3, 4},  // Note: aligned read.
          {7, 7},  // Note: both read&skip cross byte boundary.
          {17, 68},
          {7, 102},
          {9, 204},
          {3, 408},
      });

  // Set bits to one only for the first and last bit of each read
  // in the pattern.
  size_t pos = 0;
  for (auto [read_bit_count, skip_bit_count] : pattern_read_skip) {
    if (read_bit_count > 0) {
      SetBit(buffer, pos);
      SetBit(buffer, pos + read_bit_count - 1);
      pos += read_bit_count;
    }
    pos += skip_bit_count;
  }

  // Run the test.
  BitReader bit_reader(buffer);
  EXPECT_EQ(bit_reader.bits_available(), sizeof(buffer) * 8);
  for (auto [read_bit_count, skip_bit_count] : pattern_read_skip) {
    if (read_bit_count > 0) {
      uint32_t value;
      EXPECT_TRUE(bit_reader.ReadBits(read_bit_count, &value));
      EXPECT_EQ(value, 1u | (1 << (read_bit_count - 1)));
    }
    EXPECT_TRUE(bit_reader.SkipBits(skip_bit_count));
  }
}

TEST(BitReaderTest, BitsReadTest) {
  uint32_t value;
  bool flag;
  uint8_t buffer[] = {0x0a, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  BitReader reader1(buffer);
  EXPECT_EQ(reader1.bits_available(), 120u);

  EXPECT_TRUE(reader1.SkipBits(2));
  EXPECT_EQ(reader1.bits_read(), 2u);
  EXPECT_EQ(reader1.bits_available(), 118u);
  EXPECT_TRUE(reader1.ReadBits(3, &value));
  EXPECT_EQ(reader1.bits_read(), 5u);
  EXPECT_EQ(reader1.bits_available(), 115u);
  EXPECT_TRUE(reader1.ReadFlag(&flag));
  EXPECT_EQ(reader1.bits_read(), 6u);
  EXPECT_EQ(reader1.bits_available(), 114u);
  EXPECT_TRUE(reader1.SkipBits(76));
  EXPECT_EQ(reader1.bits_read(), 82u);
  EXPECT_EQ(reader1.bits_available(), 38u);
}

}  // namespace media
