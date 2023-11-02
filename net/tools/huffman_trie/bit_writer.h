// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_HUFFMAN_TRIE_BIT_WRITER_H_
#define NET_TOOLS_HUFFMAN_TRIE_BIT_WRITER_H_

#include <stdint.h>

#include <vector>

namespace net::huffman_trie {

// BitWriter acts as a buffer to which bits can be written. The bits are stored
// as bytes in a vector. BitWriter will buffer bits until it contains 8 bits at
// which point they will be appended to the vector automatically.
class BitWriter {
 public:
  BitWriter();

  BitWriter(const BitWriter&) = delete;
  BitWriter& operator=(const BitWriter&) = delete;

  ~BitWriter();

  // Appends |bit| to the end of the buffer.
  void WriteBit(uint8_t bit);

  // Appends the |number_of_bits| least-significant bits of |bits| to the end of
  // the buffer.
  void WriteBits(uint32_t bits, uint8_t number_of_bits);

  // Appends the buffered bits in |current_byte_| to the |bytes_| vector. When
  // there are less than 8 bits in the buffer, the empty bits will be filled
  // with zero's.
  void Flush();
  uint32_t position() const { return position_; }

  // Returns a reference to |bytes_|. Make sure to call Flush() first so that
  // the buffered bits are written to |bytes_| as well.
  const std::vector<uint8_t>& bytes() const { return bytes_; }

 private:
  // Buffers bits until they fill a whole byte.
  uint8_t current_byte_ = 0;

  // The number of bits currently in |current_byte_|.
  uint8_t used_ = 0;

  // Total number of bits written to this BitWriter.
  uint32_t position_ = 0;

  std::vector<uint8_t> bytes_;
};

}  // namespace net::huffman_trie

#endif  // NET_TOOLS_HUFFMAN_TRIE_BIT_WRITER_H_
