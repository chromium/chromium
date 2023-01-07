// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_HUFFMAN_TRIE_HUFFMAN_HUFFMAN_BUILDER_H_
#define NET_TOOLS_HUFFMAN_TRIE_HUFFMAN_HUFFMAN_BUILDER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

namespace net::huffman_trie {

namespace {
class HuffmanNode;
}  // namespace

struct HuffmanRepresentation {
  uint32_t bits;
  uint32_t number_of_bits;
};

// A HuffmanRepresentationTable maps the original characters to their Huffman
// representation. The Huffman representation consists of the number of bits
// needed to represent the character and the actual bits.
using HuffmanRepresentationTable = std::map<uint8_t, HuffmanRepresentation>;
using HuffmanRepresentationPair = std::pair<uint8_t, HuffmanRepresentation>;

// This class tracks the number of times each character is used and calculates
// a space efficient way to represent all tracked characters by constructing a
// Huffman tree based on the number of times each character is seen.
class HuffmanBuilder {
 public:
  HuffmanBuilder();
  ~HuffmanBuilder();

  // Will increase the count for |character| by one, indicating it has been
  // used. |character| must be in the range 0-127.
  void RecordUsage(uint8_t character);

  // Returns a HuffmanRepresentationTable based on the usage data collected
  // through RecordUsage().
  HuffmanRepresentationTable ToTable();

  // Outputs the Huffman representation as a vector of uint8_t's in a format
  // Chromium can use to reconstruct the tree.
  //
  // The nodes of the tree are pairs of uint8s. The last node in the array is
  // the root of the tree. Each pair is two uint8_t values, the first is "left"
  // and the second is "right". If a uint8_t value has the MSB set then it
  // represents a literal leaf value. Otherwise it's a pointer to the n'th
  // element of the array.
  std::vector<uint8_t> ToVector();

 private:
  // Determines the Huffman representation of the characters under |node| and
  // inserts them into |*table|. |bits| and |number_of_bits| are used as a
  // prefix.
  void TreeToTable(HuffmanNode* node,
                   uint32_t bits,
                   uint32_t number_of_bits,
                   HuffmanRepresentationTable* table);

  // Converts the tree under |*node| into a byte representation in |*vector|.
  // See ToVector() for more information on the format.
  uint32_t WriteToVector(HuffmanNode* node, std::vector<uint8_t>* vector);

  // Constructs a Huffman tree based on |counts_|. Appends additional nodes to
  // the tree until it contains at least 2 leafs.
  std::unique_ptr<HuffmanNode> BuildTree();

  // Holds usage information for the tracked characters. Maps the character to
  // the number of times its usage has been recorded through RecordUsage().
  std::map<uint8_t, uint32_t> counts_;
};

}  // namespace net::huffman_trie

#endif  // NET_TOOLS_HUFFMAN_TRIE_HUFFMAN_HUFFMAN_BUILDER_H_
