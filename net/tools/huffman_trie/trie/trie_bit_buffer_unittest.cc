// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/huffman_trie/trie/trie_bit_buffer.h"
#include "net/tools/huffman_trie/bit_writer.h"
#include "net/tools/huffman_trie/huffman/huffman_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::huffman_trie {

namespace {

// Test writing single bits to the buffer.
TEST(TrieBitBufferTest, WriteBit) {
  TrieBitBuffer buffer;

  buffer.WriteBit(0);
  buffer.WriteBit(1);
  buffer.WriteBit(0);
  buffer.WriteBit(1);
  buffer.WriteBit(0);
  buffer.WriteBit(1);
  buffer.WriteBit(0);
  buffer.WriteBit(1);

  BitWriter writer;
  buffer.WriteToBitWriter(&writer);

  writer.Flush();

  // 0 + 1 + 0 + 1 + 0 + 1 + 0 + 1 = 0x55
  EXPECT_THAT(writer.bytes(), testing::ElementsAre(0x55, 0x0));
  EXPECT_EQ(16U, writer.position());

  buffer.WriteBit(0);
  buffer.WriteBit(1);
  buffer.WriteBit(0);

  BitWriter writer2;
  buffer.WriteToBitWriter(&writer2);
  EXPECT_EQ(11U, writer2.position());

  writer2.Flush();

  // 0 + 1 + 0 + 1 + 0 + 1 + 0 + 1 + 0 + 1 + 0 + 00000 (padding) = 0x5540.
  EXPECT_THAT(writer2.bytes(), testing::ElementsAre(0x55, 0x40));
}

// Test writing multiple bits at once. Specifically, that the correct bits are
// written and byte boundaries are respected.
TEST(TrieBitBufferTest, WriteBits) {
  TrieBitBuffer buffer;

  // 0xAA is 10101010 in binary. WritBits will write the n least significant
  // bits where n is given as the second parameter.
  buffer.WriteBits(0xAA, 1);
  buffer.WriteBits(0xAA, 2);
  buffer.WriteBits(0xAA, 3);

  BitWriter writer;
  buffer.WriteToBitWriter(&writer);
  EXPECT_EQ(6U, writer.position());

  writer.Flush();

  // 0 + 10 + 010 + 00 (padding) = 0x48
  EXPECT_THAT(writer.bytes(), testing::ElementsAre(0x48));

  buffer.WriteBits(0xAA, 2);
  buffer.WriteBits(0xAA, 2);

  BitWriter writer2;
  buffer.WriteToBitWriter(&writer2);
  EXPECT_EQ(10U, writer2.position());

  writer2.Flush();

  // 0 + 10 + 010 + 10 + 10 + 000000 (padding) = 0x4A80.
  EXPECT_THAT(writer2.bytes(), testing::ElementsAre(0x4A, 0x80));

  buffer.WriteBits(0xAA, 2);

  BitWriter writer3;
  buffer.WriteToBitWriter(&writer3);
  EXPECT_EQ(12U, writer3.position());

  writer3.Flush();

  // 0 + 10 + 010 + 10 + 10 + 10 + 0000 (padding) = 0x4AA0.
  EXPECT_THAT(writer3.bytes(), testing::ElementsAre(0x4A, 0xA0));
}

// Test writing position (delta's).
TEST(TrieBitBufferTest, WritePosition) {
  TrieBitBuffer buffer;
  BitWriter writer;

  buffer.WriteBit(1);
  // 0xAA is 10101010 in binary. WritBits will write the n least significant
  // bits where n is given as the second parameter.
  buffer.WriteBits(0xAA, 6);

  buffer.WriteToBitWriter(&writer);

  TrieBitBuffer buffer2;
  int32_t last_position = -1;
  buffer2.WritePosition(4, &last_position);
  EXPECT_EQ(4, last_position);

  buffer2.WriteBits(0xAA, 8);
  buffer2.WritePosition(8, &last_position);
  EXPECT_EQ(8, last_position);

  buffer2.WriteToBitWriter(&writer);
  writer.Flush();

  EXPECT_EQ(4U, writer.bytes().size());

  // The buffer should contain, in order:
  // - the bit 1
  // - the last 6 bits of '0xAA'
  // - five bits representing '2'; the bit length of the following field
  // - 2 bits representing '3' (the delta 7 - 4)
  // - 8 bits representing 0xAA
  // - A zero indicating the following 7 bits represent a delta
  // - 7 bits representing 4 (the delta 8 - 4)
  // - padding
  //
  // 1 + 101010 + 00010 + 11 + 10101010 + 0 + 0000100 + 00 (padding)
  EXPECT_THAT(writer.bytes(), testing::ElementsAre(0xD4, 0x2E, 0xA8, 0x10));
}

// Test writing characters to the buffer using Huffman.
TEST(TrieBitBufferTest, WriteChar) {
  TrieBitBuffer buffer;
  HuffmanBuilder huffman_builder;
  HuffmanRepresentationTable table;

  table['a'] = HuffmanRepresentation();
  table['a'].bits = 0x0A;
  table['a'].number_of_bits = 4;

  table['b'] = HuffmanRepresentation();
  table['b'].bits = 0x0F;
  table['b'].number_of_bits = 4;

  buffer.WriteChar('a', table, &huffman_builder);

  HuffmanRepresentationTable encoding = huffman_builder.ToTable();

  // 'a' should have a Huffman encoding.
  EXPECT_NE(encoding.cend(), encoding.find('a'));

  buffer.WriteChar('a', table, &huffman_builder);
  buffer.WriteChar('b', table, &huffman_builder);

  encoding = huffman_builder.ToTable();

  // Both 'a' and 'b' should have a Huffman encoding.
  EXPECT_NE(encoding.cend(), encoding.find('a'));
  EXPECT_NE(encoding.cend(), encoding.find('b'));

  BitWriter writer;
  buffer.WriteToBitWriter(&writer);
  writer.Flush();

  // There should be 3 characters in the writer. 'a' twice followed by 'b' once.
  // The characters are written as the representation in |table|.
  EXPECT_EQ(2U, writer.bytes().size());

  // Twice 'a', once 'b' and padding
  EXPECT_THAT(writer.bytes(), testing::ElementsAre(0xAA, 0xF0));
}

// Test writing a mix of items. Specifically, that the correct values are
// written in the correct order and byte boundaries are respected.
TEST(TrieBitBufferTest, WriteMix) {
  TrieBitBuffer buffer;

  HuffmanRepresentationTable table;
  table['a'] = HuffmanRepresentation();
  table['a'].bits = 0x0A;
  table['a'].number_of_bits = 4;

  // 0xAA is 10101010 in binary. WritBits will write the n least significant
  // bits where n is given as the second parameter.
  buffer.WriteBits(0xAA, 1);
  buffer.WriteBit(1);

  buffer.WriteChar('a', table, nullptr);

  buffer.WriteBits(0xAA, 2);
  buffer.WriteBits(0xAA, 3);

  BitWriter writer;
  buffer.WriteToBitWriter(&writer);

  // 1 + 1 + 4 + 2 + 3 = 11.
  EXPECT_EQ(writer.position(), 11U);

  TrieBitBuffer buffer2;
  buffer2.WriteBit(1);
  buffer2.WriteBits(0xAA, 2);
  buffer2.WriteBit(0);

  buffer2.WriteToBitWriter(&writer);
  EXPECT_EQ(writer.position(), 15U);
  EXPECT_EQ(writer.bytes().size(), 1U);

  writer.Flush();

  EXPECT_EQ(writer.bytes().size(), 2U);

  // 0 + 1 + 1010 + 10 + 010 + 1 + 10 + 0 + 0 (padding) = 0x6A58.
  EXPECT_THAT(writer.bytes(), testing::ElementsAre(0x6A, 0x58));
}

}  // namespace

}  // namespace net::huffman_trie
