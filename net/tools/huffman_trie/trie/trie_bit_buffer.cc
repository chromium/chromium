// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/huffman_trie/trie/trie_bit_buffer.h"

#include <ostream>

#include "base/check.h"
#include "net/tools/huffman_trie/bit_writer.h"

namespace net {

namespace huffman_trie {

TrieBitBuffer::TrieBitBuffer() = default;

TrieBitBuffer::~TrieBitBuffer() = default;

void TrieBitBuffer::WriteBit(uint8_t bit) {
  current_byte_ |= bit << (7 - used_);
  used_++;

  if (used_ == 8) {
    Flush();
  }
}

void TrieBitBuffer::WriteBits(uint32_t bits, uint8_t number_of_bits) {
  DCHECK(number_of_bits <= 32);
  for (uint8_t i = 1; i <= number_of_bits; i++) {
    uint8_t bit = 1 & (bits >> (number_of_bits - i));
    WriteBit(bit);
  }
}

void TrieBitBuffer::WritePosition(uint32_t position, int32_t* last_position) {
  if (*last_position != -1) {
    int32_t delta = position - *last_position;
    DCHECK(delta > 0) << "delta position is not positive.";

    uint8_t number_of_bits = BitLength(delta);
    DCHECK(number_of_bits <= 7 + 15) << "positive position delta too large.";

    if (number_of_bits <= 7) {
      WriteBits(0, 1);
      WriteBits(delta, 7);
    } else {
      WriteBits(1, 1);
      WriteBits(number_of_bits - 8, 4);
      WriteBits(delta, number_of_bits);
    }

    *last_position = position;
    return;
  }

  if (used_ != 0) {
    Flush();
  }

  AppendPositionElement(position);

  *last_position = position;
}

uint8_t TrieBitBuffer::BitLength(uint32_t input) const {
  uint8_t number_of_bits = 0;
  while (input != 0) {
    number_of_bits++;
    input >>= 1;
  }
  return number_of_bits;
}

void TrieBitBuffer::WriteChar(uint8_t byte,
                              const HuffmanRepresentationTable& table,
                              HuffmanBuilder* huffman_builder) {
  HuffmanRepresentationTable::const_iterator item;
  item = table.find(byte);
  DCHECK(item != table.end());
  if (huffman_builder) {
    huffman_builder->RecordUsage(byte);
  }
  WriteBits(item->second.bits, item->second.number_of_bits);
}

void TrieBitBuffer::WriteSize(size_t size) {
  switch (size) {
    case 0:
      WriteBits(0b00, 2);
      break;
    case 1:
      WriteBits(0b100, 3);
      break;
    case 2:
      WriteBits(0b101, 3);
      break;
    case 3:
      WriteBits(0b110, 3);
      break;
    default: {
      WriteBit(size % 2);
      for (size_t len = (size + 1) / 2; len > 0; --len) {
        WriteBit(1);
      }
      WriteBit(0);
    }
  }
}

void TrieBitBuffer::AppendBitsElement(uint8_t bits, uint8_t number_of_bits) {
  BitsOrPosition element;
  element.bits = current_byte_;
  element.number_of_bits = used_;
  elements_.push_back(element);
}

void TrieBitBuffer::AppendPositionElement(uint32_t position) {
  BitsOrPosition element;
  element.position = position;
  element.number_of_bits = 0;
  elements_.push_back(element);
}

uint32_t TrieBitBuffer::WriteToBitWriter(BitWriter* writer) {
  Flush();

  uint32_t old_position = writer->position();
  for (auto const& element : elements_) {
    if (element.number_of_bits) {
      writer->WriteBits(element.bits >> (8 - element.number_of_bits),
                        element.number_of_bits);
    } else {
      uint32_t current = old_position;
      uint32_t target = element.position;
      DCHECK(target < current) << "Reference is not backwards";
      uint32_t delta = current - target;
      uint8_t delta_number_of_bits = BitLength(delta);
      DCHECK(delta_number_of_bits < 32) << "Delta to large";
      writer->WriteBits(delta_number_of_bits, 5);
      writer->WriteBits(delta, delta_number_of_bits);
    }
  }
  return old_position;
}

void TrieBitBuffer::Flush() {
  if (used_) {
    AppendBitsElement(current_byte_, used_);

    used_ = 0;
    current_byte_ = 0;
  }
}

}  // namespace huffman_trie

}  // namespace net
