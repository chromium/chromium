// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/h264_bit_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(H264BitReaderTest, ReadStreamWithoutEscapeAndTrailingZeroBytes) {
  H264BitReader reader;
  static constexpr uint8_t rbsp[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xa0};
  uint32_t dummy = 0;

  EXPECT_TRUE(reader.Initialize(rbsp));

  EXPECT_TRUE(reader.ReadBits(1, &dummy));
  EXPECT_EQ(dummy, 0x00u);
  EXPECT_EQ(reader.NumBitsLeft(), 47u);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(8, &dummy));
  EXPECT_EQ(dummy, 0x02u);
  EXPECT_EQ(reader.NumBitsLeft(), 39u);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(31, &dummy));
  EXPECT_EQ(dummy, 0x23456789u);
  EXPECT_EQ(reader.NumBitsLeft(), 8u);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(1, &dummy));
  EXPECT_EQ(dummy, 1u);
  EXPECT_EQ(reader.NumBitsLeft(), 7u);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(1, &dummy));
  EXPECT_EQ(dummy, 0u);
  EXPECT_EQ(reader.NumBitsLeft(), 6u);
  EXPECT_FALSE(reader.HasMoreRBSPData());
}

TEST(H264BitReaderTest, SingleByteStream) {
  H264BitReader reader;
  static constexpr uint8_t rbsp[] = {0x18};
  uint32_t dummy = 0;

  EXPECT_TRUE(reader.Initialize(rbsp));
  EXPECT_EQ(reader.NumBitsLeft(), 8u);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(4, &dummy));
  EXPECT_EQ(dummy, 0x01u);
  EXPECT_EQ(reader.NumBitsLeft(), 4u);
  EXPECT_FALSE(reader.HasMoreRBSPData());
}

TEST(H264BitReaderTest, StopBitOccupyFullByte) {
  H264BitReader reader;
  static constexpr uint8_t rbsp[] = {0xab, 0x80};
  uint32_t dummy = 0;

  EXPECT_TRUE(reader.Initialize(rbsp));
  EXPECT_EQ(reader.NumBitsLeft(), 16u);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(8, &dummy));
  EXPECT_EQ(dummy, 0xabu);
  EXPECT_EQ(reader.NumBitsLeft(), 8u);
  EXPECT_FALSE(reader.HasMoreRBSPData());
}

}  // namespace media
