// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/huffman_trie/bit_writer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::huffman_trie {

namespace {

// Test that single bits are written to the buffer correctly.
TEST(BitWriterTest, WriteBit) {
  BitWriter writer;

  EXPECT_EQ(0U, writer.position());
  EXPECT_EQ(0U, writer.bytes().size());

  writer.WriteBit(0);

  EXPECT_EQ(1U, writer.position());

  writer.WriteBit(1);
  writer.WriteBit(0);
  writer.WriteBit(1);
  writer.WriteBit(0);
  writer.WriteBit(1);
  writer.WriteBit(0);
  writer.WriteBit(1);

  EXPECT_EQ(8U, writer.position());

  writer.WriteBit(0);

  EXPECT_EQ(9U, writer.position());

  writer.WriteBit(1);
  writer.WriteBit(0);

  EXPECT_EQ(11U, writer.position());

  // Flush should pad the current byte with zero's until it's full.
  writer.Flush();

  // The writer should have 2 bytes now even though we only wrote 11 bits.
  EXPECT_EQ(16U, writer.position());

  // 0 + 1 + 0 + 1 + 0 + 1 + 0 + 1 + 0 + 1 + 0  + 00000 (padding) = 0x5540.
  EXPECT_THAT(writer.bytes(), testing::ElementsAre(0x55, 0x40));
}

// Test that when multiple bits are written to the buffer, they are appended
// correctly.
TEST(BitWriterTest, WriteBits) {
  BitWriter writer;

  // 0xAA is 10101010 in binary. WritBits will write the n least significant
  // bits where n is given as the second parameter.
  writer.WriteBits(0xAA, 1);
  EXPECT_EQ(1U, writer.position());
  writer.WriteBits(0xAA, 2);
  EXPECT_EQ(3U, writer.position());
  writer.WriteBits(0xAA, 3);
  EXPECT_EQ(6U, writer.position());
  writer.WriteBits(0xAA, 2);
  EXPECT_EQ(8U, writer.position());
  writer.WriteBits(0xAA, 2);
  EXPECT_EQ(10U, writer.position());

  // Flush should pad the current byte with zero's until it's full.
  writer.Flush();

  // The writer should have 2 bytes now even though we only wrote 10 bits.
  EXPECT_EQ(16U, writer.position());

  // 0 + 10 + 010 + 10 + 10 + 000000 (padding) = 0x4A80
  EXPECT_THAT(writer.bytes(), testing::ElementsAre(0x4A, 0x80));
}

// Test that buffering works correct when the methods are mixed.
TEST(BitWriterTest, WriteBoth) {
  BitWriter writer;

  // 0xAA is 10101010 in binary. WritBits will write the n least significant
  // bits where n is given as the second parameter.
  writer.WriteBits(0xAA, 1);
  EXPECT_EQ(1U, writer.position());
  writer.WriteBit(1);
  writer.WriteBits(0xAA, 2);
  EXPECT_EQ(4U, writer.position());
  writer.WriteBits(0xAA, 3);
  EXPECT_EQ(7U, writer.position());
  writer.WriteBit(1);
  EXPECT_EQ(8U, writer.position());

  writer.WriteBits(0xAA, 2);
  writer.WriteBit(0);
  EXPECT_EQ(11U, writer.position());

  // Flush should pad the current byte with zero's until it's full.
  writer.Flush();

  // The writer should have 2 bytes now even though we only wrote 10 bits.
  EXPECT_EQ(16U, writer.position());

  // 0 + 1 + 10 + 010 + 1 + 10 + 0 + 00000 (padding) = 0x6580
  EXPECT_THAT(writer.bytes(), testing::ElementsAre(0x65, 0x80));
}

}  // namespace

}  // namespace net::huffman_trie
