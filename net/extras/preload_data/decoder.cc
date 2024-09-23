// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/extras/preload_data/decoder.h"
#include "base/check_op.h"
#include "base/notreached.h"

namespace net::extras {

PreloadDecoder::BitReader::BitReader(const uint8_t* bytes, size_t num_bits)
    : bytes_(bytes), num_bits_(num_bits), num_bytes_((num_bits + 7) / 8) {}

// Next sets |*out| to the next bit from the input. It returns false if no
// more bits are available or true otherwise.
bool PreloadDecoder::BitReader::Next(bool* out) {
  if (num_bits_used_ == 8) {
    if (current_byte_index_ >= num_bytes_) {
      return false;
    }
    current_byte_ = bytes_[current_byte_index_++];
    num_bits_used_ = 0;
  }

  *out = 1 & (current_byte_ >> (7 - num_bits_used_));
  num_bits_used_++;
  return true;
}

// Read sets the |num_bits| least-significant bits of |*out| to the value of
// the next |num_bits| bits from the input. It returns false if there are
// insufficient bits in the input or true otherwise.
bool PreloadDecoder::BitReader::Read(unsigned num_bits, uint32_t* out) {
  DCHECK_LE(num_bits, 32u);

  uint32_t ret = 0;
  for (unsigned i = 0; i < num_bits; ++i) {
    bool bit;
    if (!Next(&bit)) {
      return false;
    }
    ret |= static_cast<uint32_t>(bit) << (num_bits - 1 - i);
  }

  *out = ret;
  return true;
}

namespace {

// Reads one bit from |reader|, shifts |*bits| left by 1, and adds the read bit
// to the end of |*bits|.
bool ReadBit(PreloadDecoder::BitReader* reader, uint8_t* bits) {
  bool bit;
  if (!reader->Next(&bit)) {
    return false;
  }
  *bits <<= 1;
  if (bit) {
    (*bits)++;
  }
  return true;
}

}  // namespace

bool PreloadDecoder::BitReader::DecodeSize(size_t* out) {
  uint8_t bits = 0;
  if (!ReadBit(this, &bits) || !ReadBit(this, &bits)) {
    return false;
  }
  if (bits == 0) {
    *out = 0;
    return true;
  }
  if (!ReadBit(this, &bits)) {
    return false;
  }
  // We've parsed 3 bits so far. Check all possible combinations:
  bool is_even;
  switch (bits) {
    case 0b000:
    case 0b001:
      // This should have been handled in the if (bits == 0) check.
      NOTREACHED_IN_MIGRATION();
      return false;
    case 0b010:
      // A specialization of the 0b01 prefix for unary-like even numbers.
      *out = 4;
      return true;
    case 0b011:
      // This will be handled with the prefixes for unary-like encoding below.
      is_even = true;
      break;
    case 0b100:
      *out = 1;
      return true;
    case 0b101:
      *out = 2;
      return true;
    case 0b110:
      *out = 3;
      return true;
    case 0b111:
      // This will be handled with the prefixes for unary-like encoding below.
      is_even = false;
      break;
    default:
      // All cases should be covered above.
      NOTREACHED_IN_MIGRATION();
      return false;
  }
  size_t bit_length = 3;
  while (true) {
    bit_length++;
    bool bit;
    if (!Next(&bit)) {
      return false;
    }
    if (!bit) {
      break;
    }
  }
  size_t ret = (bit_length - 2) * 2;
  if (!is_even) {
    ret--;
  }
  *out = ret;
  return true;
}

// Seek sets the current offest in the input to bit number |offset|. It
// returns true if |offset| is within the range of the input and false
// otherwise.
bool PreloadDecoder::BitReader::Seek(size_t offset) {
  if (offset >= num_bits_) {
    return false;
  }
  current_byte_index_ = offset / 8;
  current_byte_ = bytes_[current_byte_index_++];
  num_bits_used_ = offset % 8;
  return true;
}

PreloadDecoder::HuffmanDecoder::HuffmanDecoder(const uint8_t* tree,
                                               size_t tree_bytes)
    : tree_(tree), tree_bytes_(tree_bytes) {}

bool PreloadDecoder::HuffmanDecoder::Decode(PreloadDecoder::BitReader* reader,
                                            char* out) const {
  const uint8_t* current = &tree_[tree_bytes_ - 2];

  for (;;) {
    bool bit;
    if (!reader->Next(&bit)) {
      return false;
    }

    uint8_t b = current[bit];
    if (b & 0x80) {
      *out = static_cast<char>(b & 0x7f);
      return true;
    }

    unsigned offset = static_cast<unsigned>(b) * 2;
    DCHECK_LT(offset, tree_bytes_);
    if (offset >= tree_bytes_) {
      return false;
    }

    current = &tree_[offset];
  }
}

PreloadDecoder::PreloadDecoder(const uint8_t* huffman_tree,
                               size_t huffman_tree_size,
                               const uint8_t* trie,
                               size_t trie_bits,
                               size_t trie_root_position)
    : huffman_decoder_(huffman_tree, huffman_tree_size),
      bit_reader_(trie, trie_bits),
      trie_root_position_(trie_root_position) {}

PreloadDecoder::~PreloadDecoder() = default;

bool PreloadDecoder::Decode(const std::string& search, bool* out_found) {
  size_t bit_offset = trie_root_position_;
  *out_found = false;

  // current_search_offset contains one more than the index of the current
  // character in the search keyword that is being considered. It's one greater
  // so that we can represent the position just before the beginning (with
  // zero).
  size_t current_search_offset = search.size();

  for (;;) {
    // Seek to the desired location.
    if (!bit_reader_.Seek(bit_offset)) {
      return false;
    }

    // Decode the length of the common prefix.
    size_t prefix_length;
    if (!bit_reader_.DecodeSize(&prefix_length)) {
      return false;
    }

    // Match each character in the prefix.
    for (size_t i = 0; i < prefix_length; ++i) {
      if (current_search_offset == 0) {
        // We can't match the terminator with a prefix string.
        return true;
      }

      char c;
      if (!huffman_decoder_.Decode(&bit_reader_, &c)) {
        return false;
      }
      if (search[current_search_offset - 1] != c) {
        return true;
      }
      current_search_offset--;
    }

    bool is_first_offset = true;
    size_t current_offset = 0;

    // Next is the dispatch table.
    for (;;) {
      char c;
      if (!huffman_decoder_.Decode(&bit_reader_, &c)) {
        return false;
      }
      if (c == kEndOfTable) {
        // No exact match.
        return true;
      }

      if (c == kEndOfString) {
        if (!ReadEntry(&bit_reader_, search, current_search_offset,
                       out_found)) {
          return false;
        }
        if (current_search_offset == 0) {
          CHECK(*out_found);
          return true;
        }
        continue;
      }

      // The entries in a dispatch table are in order thus we can tell if there
      // will be no match if the current character past the one that we want.
      if (current_search_offset == 0 || search[current_search_offset - 1] < c) {
        return true;
      }

      if (is_first_offset) {
        // The first offset is backwards from the current position.
        uint32_t jump_delta_bits;
        uint32_t jump_delta;
        if (!bit_reader_.Read(5, &jump_delta_bits) ||
            !bit_reader_.Read(jump_delta_bits, &jump_delta)) {
          return false;
        }

        if (bit_offset < jump_delta) {
          return false;
        }

        current_offset = bit_offset - jump_delta;
        is_first_offset = false;
      } else {
        // Subsequent offsets are forward from the target of the first offset.
        uint32_t is_long_jump;
        if (!bit_reader_.Read(1, &is_long_jump)) {
          return false;
        }

        uint32_t jump_delta;
        if (!is_long_jump) {
          if (!bit_reader_.Read(7, &jump_delta)) {
            return false;
          }
        } else {
          uint32_t jump_delta_bits;
          if (!bit_reader_.Read(4, &jump_delta_bits) ||
              !bit_reader_.Read(jump_delta_bits + 8, &jump_delta)) {
            return false;
          }
        }

        current_offset += jump_delta;
        if (current_offset >= bit_offset) {
          return false;
        }
      }

      DCHECK_LT(0u, current_search_offset);
      if (search[current_search_offset - 1] == c) {
        bit_offset = current_offset;
        current_search_offset--;
        break;
      }
    }
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace net::extras
