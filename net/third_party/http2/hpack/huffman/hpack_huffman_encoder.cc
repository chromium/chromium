// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/http2/hpack/huffman/hpack_huffman_encoder.h"

#include "base/logging.h"
#include "net/third_party/http2/hpack/huffman/huffman_spec_tables.h"

// TODO(jamessynge): Remove use of binary literals, that is a C++ 14 feature.

namespace http2 {

size_t ExactHuffmanSize(Http2StringPiece plain) {
  size_t bits = 0;
  for (const uint8_t c : plain) {
    bits += HuffmanSpecTables::kCodeLengths[c];
  }
  return (bits + 7) / 8;
}

size_t BoundedHuffmanSize(Http2StringPiece plain) {
  // TODO(jamessynge): Determine whether we should set the min size for Huffman
  // encoding much higher (i.e. if less than N, then the savings isn't worth
  // the cost of encoding and decoding). Of course, we need to decide on a
  // value function, which might be throughput on a full load test, or a
  // microbenchmark of the time to encode and then decode a HEADERS frame,
  // possibly with the cost of crypto included (i.e. crypto is going to have
  // a fairly constant per-byte cost, so reducing the number of bytes in-transit
  // reduces the number that must be encrypted and later decrypted).
  if (plain.size() < 3) {
    // Huffman encoded string can't be smaller than the plain size for very
    // short strings.
    return plain.size();
  }
  // TODO(jamessynge): Measure whether this can be done more efficiently with
  // nested loops (e.g. make exact measurement of 8 bytes, then check if min
  // remaining is too long).
  // Compute the number of bits in an encoding that is shorter than the plain
  // string (i.e. the number of bits in a string 1 byte shorter than plain),
  // and use this as the limit of the size of the encoding.
  const size_t limit_bits = (plain.size() - 1) * 8;
  // The shortest code length in the Huffman table of the HPACK spec has 5 bits
  // (e.g. for 0, 1, a and e).
  const size_t min_code_length = 5;
  // We can therefore say that all plain text bytes whose code length we've not
  // yet looked up will take at least 5 bits.
  size_t min_bits_remaining = plain.size() * min_code_length;
  size_t bits = 0;
  for (const uint8_t c : plain) {
    bits += HuffmanSpecTables::kCodeLengths[c];
    min_bits_remaining -= min_code_length;
    // If our minimum estimate of the total number of bits won't yield an
    // encoding shorter the plain text, let's bail.
    const size_t minimum_bits_total = bits + min_bits_remaining;
    if (minimum_bits_total > limit_bits) {
      bits += min_bits_remaining;
      break;
    }
  }
  return (bits + 7) / 8;
}

void HuffmanEncode(Http2StringPiece plain, Http2String* huffman) {
  DCHECK(huffman != nullptr);
  huffman->clear();         // Note that this doesn't release memory.
  uint64_t bit_buffer = 0;  // High-bit is next bit to output. Not clear if that
                            // is more performant than having the low-bit be the
                            // last to be output.
  size_t bits_unused = 64;  // Number of bits available for the next code.
  for (uint8_t c : plain) {
    size_t code_length = HuffmanSpecTables::kCodeLengths[c];
    if (bits_unused < code_length) {
      // There isn't enough room in bit_buffer for the code of c.
      // Flush until bits_unused > 56 (i.e. 64 - 8).
      do {
        char h = static_cast<char>(bit_buffer >> 56);
        bit_buffer <<= 8;
        bits_unused += 8;
        // Perhaps would be more efficient if we populated an array of chars,
        // so we don't have to call push_back each time. Reconsider if used
        // for production.
        huffman->push_back(h);
      } while (bits_unused <= 56);
    }
    uint64_t code = HuffmanSpecTables::kRightCodes[c];
    size_t shift_by = bits_unused - code_length;
    bit_buffer |= (code << shift_by);
    bits_unused -= code_length;
  }
  // bit_buffer contains (64-bits_unused) bits that still need to be flushed.
  // Output whole bytes until we don't have any whole bytes left.
  size_t bits_used = 64 - bits_unused;
  while (bits_used >= 8) {
    char h = static_cast<char>(bit_buffer >> 56);
    bit_buffer <<= 8;
    bits_used -= 8;
    huffman->push_back(h);
  }
  if (bits_used > 0) {
    // We have less than a byte left to output. The spec calls for padding out
    // the final byte with the leading bits of the EOS symbol (30 1-bits).
    constexpr uint64_t leading_eos_bits = 0b11111111;
    bit_buffer |= (leading_eos_bits << (56 - bits_used));
    char h = static_cast<char>(bit_buffer >> 56);
    huffman->push_back(h);
  }
}

}  // namespace http2
