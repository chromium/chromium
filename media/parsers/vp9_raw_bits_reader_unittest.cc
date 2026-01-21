// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <tuple>

#include "media/parsers/vp9_raw_bits_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(Vp9RawBitsReaderTest, ReadBool) {
  uint8_t data[] = {0xf1};
  Vp9RawBitsReader reader;
  reader.Initialize(data, 1);

  EXPECT_TRUE(reader.IsValid());
  EXPECT_EQ(0u, reader.GetBytesRead());
  EXPECT_TRUE(reader.ReadBool());
  EXPECT_EQ(1u, reader.GetBytesRead());
  EXPECT_TRUE(reader.ReadBool());
  EXPECT_TRUE(reader.ReadBool());
  EXPECT_TRUE(reader.ReadBool());
  EXPECT_FALSE(reader.ReadBool());
  EXPECT_FALSE(reader.ReadBool());
  EXPECT_FALSE(reader.ReadBool());
  EXPECT_TRUE(reader.ReadBool());
  EXPECT_TRUE(reader.IsValid());

  // The return value is undefined.
  std::ignore = reader.ReadBool();
  EXPECT_FALSE(reader.IsValid());
  EXPECT_EQ(1u, reader.GetBytesRead());
}

TEST(Vp9RawBitsReader, ReadLiteral) {
  uint8_t data[] = {0x3d, 0x67, 0x9a};
  Vp9RawBitsReader reader;
  reader.Initialize(data, 3);

  EXPECT_TRUE(reader.IsValid());
  EXPECT_EQ(0x03, reader.ReadLiteral(4));
  EXPECT_EQ(0xd679, reader.ReadLiteral(16));
  EXPECT_TRUE(reader.IsValid());

  // The return value is undefined.
  std::ignore = reader.ReadLiteral(8);
  EXPECT_FALSE(reader.IsValid());
  EXPECT_EQ(3u, reader.GetBytesRead());
}

TEST(Vp9RawBitsReader, ReadSignedLiteral) {
  uint8_t data[] = {0x3d, 0x67, 0x9a};
  Vp9RawBitsReader reader;
  reader.Initialize(data, 3);

  EXPECT_TRUE(reader.IsValid());
  EXPECT_EQ(-0x03, reader.ReadSignedLiteral(4));
  EXPECT_EQ(-0x5679, reader.ReadSignedLiteral(15));
  EXPECT_TRUE(reader.IsValid());

  // The return value is undefined.
  std::ignore = reader.ReadSignedLiteral(7);
  EXPECT_FALSE(reader.IsValid());
  EXPECT_EQ(3u, reader.GetBytesRead());
}

}  // namespace media
