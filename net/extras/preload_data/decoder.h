// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_EXTRAS_PRELOAD_DATA_DECODER_H_
#define NET_EXTRAS_PRELOAD_DATA_DECODER_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"

namespace net {

namespace extras {

// Decodes an entry from preloaded data.
// Clients must implement ReadEntry() method to read the specific type of data
// they are interested in.
class PreloadDecoder {
 public:
  // These must match the values in net/tools/huffman_trie/trie/trie_writer.h.
  enum : char { kEndOfString = 0, kEndOfTable = 127 };

  // BitReader is a class that allows a bytestring to be read bit-by-bit.
  class BitReader {
   public:
    BitReader(const uint8_t* bytes, size_t num_bits);

    // Next sets |*out| to the next bit from the input. It returns false if no
    // more bits are available or true otherwise.
    bool Next(bool* out);

    // Read sets the |num_bits| least-significant bits of |*out| to the value of
    // the next |num_bits| bits from the input. It returns false if there are
    // insufficient bits in the input or true otherwise.
    bool Read(unsigned num_bits, uint32_t* out);

    // Decodes a size_t from the reader, putting the resulting value in |*out|.
    // Returns false if there are insufficient bits to read and true otherwise.
    //
    // This function's inverse is TrieBitBuffer::WriteSize.
    //
    // The encoding is a prefix code optimized for small values (less than 4).
    // It is designed for the lengths of prefixes in the HSTS Preload list trie.
    // Compared to the unary encoding that was previously used (where the number
    // of bits used is one plus the value being encoded), this uses one more bit
    // for encoding 0 and 1, and the same number of bits for encoding 2, and
    // fewer bits for encoding values greater than 2. At the time of writing,
    // 35% of the lengths encoded in the trie were 0 or 1, 11% were 2, and the
    // remaining 54% were greater than 2.
    //
    // This encoding scheme uses a variable number of bits to encode each value.
    // There are fixed values for 0, 1, 2, and 3, and then a simple rule is used
    // for 4 and greater. 0 uses 2 bits; 1 through 3 use 3 bits. The fixed
    // values are as follows:
    //
    //   0: 0b00
    //   1: 0b100
    //   2: 0b101
    //   3: 0b110
    //
    // Note that none of the fixed values are prefixed with 0b01 or 0b111. These
    // prefixes are used with a unary-like encoding for values 4 and above.
    // Zero or more 1s, followed by a 0, are appended to one of those prefixes.
    // Even values use the prefix 0b01, and odd values use the prefix 0b111. The
    // number of 1s to append is half the value (rounded down) minus 1.
    bool DecodeSize(size_t* out);

    // Seek sets the current offest in the input to bit number |offset|. It
    // returns true if |offset| is within the range of the input and false
    // otherwise.
    bool Seek(size_t offset);

   private:
    const uint8_t* const bytes_;
    const size_t num_bits_;
    const size_t num_bytes_;
    // current_byte_index_ contains the current byte offset in |bytes_|.
    size_t current_byte_index_;
    // current_byte_ contains the current byte of the input.
    uint8_t current_byte_;
    // num_bits_used_ contains the number of bits of |current_byte_| that have
    // been read.
    unsigned num_bits_used_;

    DISALLOW_COPY_AND_ASSIGN(BitReader);
  };

  // HuffmanDecoder is a very simple Huffman reader. The input Huffman tree is
  // simply encoded as a series of two-byte structures. The first byte
  // determines the "0" pointer for that node and the second the "1" pointer.
  // Each byte either has the MSB set, in which case the bottom 7 bits are the
  // value for that position, or else the bottom seven bits contain the index of
  // a node.
  //
  // The tree is decoded by walking rather than a table-driven approach.
  class HuffmanDecoder {
   public:
    HuffmanDecoder(const uint8_t* tree, size_t tree_bytes);

    bool Decode(PreloadDecoder::BitReader* reader, char* out) const;

   private:
    const uint8_t* const tree_;
    const size_t tree_bytes_;

    DISALLOW_COPY_AND_ASSIGN(HuffmanDecoder);
  };

  PreloadDecoder(const uint8_t* huffman_tree,
                 size_t huffman_tree_size,
                 const uint8_t* trie,
                 size_t trie_bits,
                 size_t trie_root_position);
  virtual ~PreloadDecoder();

  // Resolves search keyword given by |search| in the preloaded data. Returns
  // false on internal error and true otherwise. After a successful return,
  // |*out_found| is true iff a relevant entry has been found. In the case of
  // HSTS data, |search| is the hostname being searched.
  //
  // Although this code should be robust, it never processes attacker-controlled
  // data -- it only operates on the preloaded data built into the binary.
  //
  // The preloaded data is represented as a trie and matches |search|
  // backwards. Each node in the trie starts with a number of characters, which
  // must match exactly. After that is a dispatch table which maps the next
  // character in the search keyword to another node in the trie.
  //
  // In the dispatch table, the zero character represents the "end of string"
  // (which is the *beginning* of the search keyword since we process it
  // backwards). The value in that case is special -- rather than an offset to
  // another trie node, it contains the searched entry (for HSTS data, it
  // contains whether subdomains are included, pinsets etc.). Clients must
  // implement ReadEntry to read the entry at this location.
  //
  // Dispatch tables are always given in order, but the "end of string" (zero)
  // value always comes before an entry for '.'.
  bool Decode(const std::string& search, bool* out_found);

 protected:
  virtual bool ReadEntry(BitReader* reader,
                         const std::string& search,
                         size_t current_search_offset,
                         bool* out_found) = 0;

  const HuffmanDecoder& huffman_decoder() const { return huffman_decoder_; }

 private:
  HuffmanDecoder huffman_decoder_;
  BitReader bit_reader_;

  const size_t trie_root_position_;

  DISALLOW_COPY_AND_ASSIGN(PreloadDecoder);
};

}  // namespace extras

}  // namespace net

#endif  // NET_EXTRAS_PRELOAD_DATA_DECODER_H_
