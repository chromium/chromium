// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_HUFFMAN_TRIE_TRIE_TRIE_WRITER_H_
#define NET_TOOLS_HUFFMAN_TRIE_TRIE_TRIE_WRITER_H_

#include <vector>

#include "net/tools/huffman_trie/bit_writer.h"
#include "net/tools/huffman_trie/huffman/huffman_builder.h"
#include "net/tools/huffman_trie/trie_entry.h"

namespace net::huffman_trie {

enum : uint8_t { kTerminalValue = 0, kEndOfTableValue = 127 };

class TrieWriter {
 public:
  TrieWriter(const HuffmanRepresentationTable& huffman_table,
             HuffmanBuilder* huffman_builder);
  ~TrieWriter();

  // Constructs a trie containing all |entries|. The output is written to
  // |buffer_| and |*position| is set to the position of the trie root. Returns
  // true on success and false on failure.
  bool WriteEntries(const TrieEntries& entries, uint32_t* position);

  // Returns the position |buffer_| is currently at. The returned value
  // represents the number of bits.
  uint32_t position() const;

  // Flushes |buffer_|.
  void Flush();

  // Returns the trie bytes. Call Flush() first to ensure the buffer is
  // complete.
  const std::vector<uint8_t>& bytes() const { return buffer_.bytes(); }

 protected:
  const HuffmanRepresentationTable& huffman_table() const {
    return huffman_table_;
  }

  HuffmanBuilder* huffman_builder() { return huffman_builder_; }

 private:
  bool WriteDispatchTables(ReversedEntries::iterator start,
                           ReversedEntries::iterator end,
                           uint32_t* position);

  BitWriter buffer_;
  const HuffmanRepresentationTable& huffman_table_;
  HuffmanBuilder* huffman_builder_;
};

}  // namespace net::huffman_trie

#endif  // NET_TOOLS_HUFFMAN_TRIE_TRIE_TRIE_WRITER_H_
