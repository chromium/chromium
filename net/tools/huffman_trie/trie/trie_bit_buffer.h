// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_HUFFMAN_TRIE_TRIE_TRIE_BIT_BUFFER_H_
#define NET_TOOLS_HUFFMAN_TRIE_TRIE_TRIE_BIT_BUFFER_H_

#include <stdint.h>

#include <vector>

#include "net/tools/huffman_trie/huffman/huffman_builder.h"

namespace net::huffman_trie {

class BitWriter;

// TrieBitBuffer acts as a buffer for TrieWriter. It can be used to write bits,
// characters, and positions. The characters are stored as their
// HuffmanRepresentation. Positions are references to other locations in the
// trie.
class TrieBitBuffer {
 public:
  TrieBitBuffer();

  TrieBitBuffer(const TrieBitBuffer&) = delete;
  TrieBitBuffer& operator=(const TrieBitBuffer&) = delete;

  ~TrieBitBuffer();

  // Writes |bit| to the buffer.
  void WriteBit(uint8_t bit);

  // Writes the |number_of_bits| least-significant bits from |bits| to the
  // buffer.
  void WriteBits(uint32_t bits, uint8_t number_of_bits);

  // Write a position to the buffer. Actually writes the difference between
  // |position| and |*last_position|. |*last_position| will be updated to equal
  // the input |position|.
  void WritePosition(uint32_t position, int32_t* last_position);

  // Writes the character in |byte| to the buffer using its Huffman
  // representation in |table|. Optionally tracks usage of the character in
  // |*huffman_builder|.
  void WriteChar(uint8_t byte,
                 const HuffmanRepresentationTable& table,
                 HuffmanBuilder* huffman_builder);

  // Writes a size_t |size| in a format that provides a compact representation
  // for small values. This function's inverse is PreloadDecoder::DecodeSize.
  void WriteSize(size_t size);

  // Writes the entire buffer to |*writer|. Returns the position |*writer| was
  // at before the buffer was written to it.
  uint32_t WriteToBitWriter(BitWriter* writer);

  // Appends the buffered bits in |current_byte_| to |elements_|. No padding
  // will occur.
  void Flush();

 private:
  // Represents either the |number_of_bits| least-significant bits in |bits| or
  // a position (offset) in the trie.
  struct BitsOrPosition {
    uint8_t bits;
    uint8_t number_of_bits;
    uint32_t position;
  };

  // Append a new element to |elements_|.
  void AppendBitsElement(uint8_t bits, uint8_t number_of_bits);
  void AppendPositionElement(uint32_t position);

  // Buffers bits until they fill a whole byte.
  uint8_t current_byte_ = 0;

  // The number of bits currently in |current_byte_|.
  uint32_t used_ = 0;

  std::vector<BitsOrPosition> elements_;
};

}  // namespace net::huffman_trie

#endif  // NET_TOOLS_HUFFMAN_TRIE_TRIE_TRIE_BIT_BUFFER_H_
