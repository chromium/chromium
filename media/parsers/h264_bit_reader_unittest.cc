// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/h264_bit_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(H264BitReaderTest, ReadStreamWithoutEscapeAndTrailingZeroBytes) {
  H264BitReader reader;
  const unsigned char rbsp[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xa0};
  int dummy = 0;

  EXPECT_TRUE(reader.Initialize(rbsp, sizeof(rbsp)));

  EXPECT_TRUE(reader.ReadBits(1, &dummy));
  EXPECT_EQ(dummy, 0x00);
  EXPECT_EQ(reader.NumBitsLeft(), 47);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(8, &dummy));
  EXPECT_EQ(dummy, 0x02);
  EXPECT_EQ(reader.NumBitsLeft(), 39);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(31, &dummy));
  EXPECT_EQ(dummy, 0x23456789);
  EXPECT_EQ(reader.NumBitsLeft(), 8);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(1, &dummy));
  EXPECT_EQ(dummy, 1);
  EXPECT_EQ(reader.NumBitsLeft(), 7);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(1, &dummy));
  EXPECT_EQ(dummy, 0);
  EXPECT_EQ(reader.NumBitsLeft(), 6);
  EXPECT_FALSE(reader.HasMoreRBSPData());
}

TEST(H264BitReaderTest, SingleByteStream) {
  H264BitReader reader;
  const unsigned char rbsp[] = {0x18};
  int dummy = 0;

  EXPECT_TRUE(reader.Initialize(rbsp, sizeof(rbsp)));
  EXPECT_EQ(reader.NumBitsLeft(), 8);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(4, &dummy));
  EXPECT_EQ(dummy, 0x01);
  EXPECT_EQ(reader.NumBitsLeft(), 4);
  EXPECT_FALSE(reader.HasMoreRBSPData());
}

TEST(H264BitReaderTest, StopBitOccupyFullByte) {
  H264BitReader reader;
  const unsigned char rbsp[] = {0xab, 0x80};
  int dummy = 0;

  EXPECT_TRUE(reader.Initialize(rbsp, sizeof(rbsp)));
  EXPECT_EQ(reader.NumBitsLeft(), 16);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(8, &dummy));
  EXPECT_EQ(dummy, 0xab);
  EXPECT_EQ(reader.NumBitsLeft(), 8);
  EXPECT_FALSE(reader.HasMoreRBSPData());
}

}  // namespace media
