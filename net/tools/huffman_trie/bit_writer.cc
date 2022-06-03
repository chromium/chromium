// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/huffman_trie/bit_writer.h"

#include "base/check.h"

namespace net {

namespace huffman_trie {

BitWriter::BitWriter() = default;

BitWriter::~BitWriter() = default;

void BitWriter::WriteBits(uint32_t bits, uint8_t number_of_bits) {
  DCHECK(number_of_bits <= 32);
  for (uint8_t i = 1; i <= number_of_bits; i++) {
    uint8_t bit = 1 & (bits >> (number_of_bits - i));
    WriteBit(bit);
  }
}

void BitWriter::WriteBit(uint8_t bit) {
  current_byte_ |= bit << (7 - used_);
  used_++;
  position_++;

  if (used_ == 8) {
    Flush();
  }
}

void BitWriter::Flush() {
  position_ += (8 - used_);
  bytes_.push_back(current_byte_);

  used_ = 0;
  current_byte_ = 0;
}

}  // namespace huffman_trie

}  // namespace net
