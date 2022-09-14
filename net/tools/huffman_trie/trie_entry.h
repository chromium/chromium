// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_HUFFMAN_TRIE_TRIE_ENTRY_H_
#define NET_TOOLS_HUFFMAN_TRIE_TRIE_ENTRY_H_

#include <memory>
#include <string>
#include <vector>

namespace net::huffman_trie {

class TrieBitBuffer;

class TrieEntry {
 public:
  TrieEntry();
  virtual ~TrieEntry();

  // The name to be used when inserting the entry to the trie. E.g. for HSTS
  // preload list, this is the hostname.
  virtual std::string name() const = 0;
  virtual bool WriteEntry(huffman_trie::TrieBitBuffer* writer) const = 0;
};

// std::unique_ptr's are not covariant, so operations on TrieEntry uses a vector
// of raw pointers instead.
using TrieEntries = std::vector<TrieEntry*>;

// ReversedEntry points to a TrieEntry and contains the reversed name for
// that entry. This is used to construct the trie.
struct ReversedEntry {
  ReversedEntry(std::vector<uint8_t> reversed_name, const TrieEntry* entry);
  ~ReversedEntry();

  std::vector<uint8_t> reversed_name;
  const TrieEntry* entry;
};

using ReversedEntries = std::vector<std::unique_ptr<ReversedEntry>>;

}  // namespace net::huffman_trie

#endif  // NET_TOOLS_HUFFMAN_TRIE_TRIE_ENTRY_H_
