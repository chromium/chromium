// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/huffman_trie/huffman/huffman_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::huffman_trie {

namespace {

// Test that there are no Huffman representations that are a prefix for another.
TEST(HuffmanBuilderTest, NoPrefixCollision) {
  HuffmanBuilder builder;
  HuffmanRepresentationTable encoding;
  for (uint8_t i = 0; i <= 127; i++) {
    // Make sure all values have an identical count to at least some other
    // values.
    for (uint8_t j = 0; j <= i % 32; j++) {
      builder.RecordUsage(i);
    }
  }

  encoding = builder.ToTable();
  for (uint8_t i = 0; i <= 127; i++) {
    // There should never exist a representation that is a prefix for, or
    // identical to, another.
    uint32_t mask = 0;
    for (uint32_t k = 0; k <= encoding[i].number_of_bits; k++) {
      mask = (mask << 1) | 1;
    }
    mask = mask << (32 - encoding[i].number_of_bits);

    for (uint8_t j = 0; j <= 127; j++) {
      if (i == j) {
        continue;
      }

      uint32_t aligned_i = encoding[i].bits
                           << (32 - encoding[i].number_of_bits);
      uint32_t aligned_j = encoding[j].bits
                           << (32 - encoding[j].number_of_bits);
      EXPECT_NE(aligned_i, aligned_j & mask);
    }
  }
}

// Test that all recorded characters get a representation and that no other
// representations are created.
// Note: There is an exception for encodings with less than 2 unique inputs.
TEST(HuffmanBuilderTest, NoMissingInputs) {
  HuffmanBuilder builder;
  HuffmanRepresentationTable encoding;
  for (uint8_t i = 0; i <= 127; i++) {
    if (i % 2) {
      for (uint8_t j = 0; j <= i % 5; j++) {
        builder.RecordUsage(i);
      }
    }
  }

  encoding = builder.ToTable();
  for (uint8_t i = 0; i <= 127; i++) {
    if (i % 2) {
      EXPECT_NE(encoding.find(i), encoding.cend());
    } else {
      EXPECT_EQ(encoding.find(i), encoding.cend());
    }
  }
}

// Test that the representations have optimal order by checking that characters
// with higher counts get shorter (or equal length) representations than those
// with lower counts.
TEST(HuffmanBuilderTest, OptimalCodeOrder) {
  HuffmanBuilder builder;
  HuffmanRepresentationTable encoding;
  for (uint8_t i = 0; i <= 127; i++) {
    for (uint8_t j = 0; j <= (i + 1); j++) {
      builder.RecordUsage(i);
    }
  }

  encoding = builder.ToTable();
  for (uint8_t i = 0; i <= 127; i++) {
    // The representation for |i| should be longer or have the same length as
    // all following representations because they have a higher frequency and
    // therefor should never get a longer representation.
    for (uint8_t j = i; j <= 127; j++) {
      // A representation for the values should exist in the table.
      ASSERT_NE(encoding.find(i), encoding.cend());
      ASSERT_NE(encoding.find(j), encoding.cend());

      EXPECT_GE(encoding[i].number_of_bits, encoding[j].number_of_bits);
    }
  }
}

// Test that the ToVector() creates a byte vector that represents the expected
// Huffman Tree.
TEST(HuffmanBuilderTest, ToVector) {
  // Build a small tree.
  HuffmanBuilder builder;
  builder.RecordUsage('a');
  builder.RecordUsage('b');
  builder.RecordUsage('b');
  builder.RecordUsage('c');
  builder.RecordUsage('c');
  builder.RecordUsage('d');
  builder.RecordUsage('d');
  builder.RecordUsage('d');
  builder.RecordUsage('e');
  builder.RecordUsage('e');
  builder.RecordUsage('e');

  std::vector<uint8_t> output = builder.ToVector();

  // This represents 4 nodes (4 groups of 2 uint8_t's) which, when decoded,
  // yields the expected Huffman Tree:
  //                      root (node 3)
  //                     /             \
  //              node 1                 node 2
  //            /       \               /      \
  //         0xE3 (c)    node 0     0xE4 (d)    0xE5 (e)
  //                    /      \
  //                0xE1 (a)    0xE2 (b)
  EXPECT_THAT(output, testing::ElementsAre(0xE1, 0xE2, 0xE3, 0x0, 0xE4, 0xE5,
                                           0x1, 0x2));
}

// The ToVector() logic requires at least 2 unique inputs to construct the
// vector. Test that nodes are appended when there are less than 2 unique
// inputs.
TEST(HuffmanBuilderTest, ToVectorSingle) {
  // Build a single element tree. Another element should be added automatically.
  HuffmanBuilder builder;
  builder.RecordUsage('a');

  std::vector<uint8_t> output = builder.ToVector();

  // This represents 1 node (1 group of 2 uint8_t's) which, when decoded,
  // yields the expected Huffman Tree:
  //                     root (node 0)
  //                     /           \
  //             0x80 (\0)           0xE1 (a)
  //
  // Note: the node \0 node was appended to the tree.
  EXPECT_THAT(output, testing::ElementsAre(0x80, 0xE1));
}

}  // namespace

}  // namespace net::huffman_trie
