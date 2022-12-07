// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_TRANSPORT_SECURITY_STATE_ENTRY_H_
#define NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_TRANSPORT_SECURITY_STATE_ENTRY_H_

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "net/tools/huffman_trie/trie_entry.h"

namespace net::transport_security_state {

// Maps a name to an index. This is used to track the index of several values
// in the C++ code. The trie refers to the array index of the values. For
// example; the pinsets are outputted as a C++ array and the index for the
// pinset in that array is placed in the trie.
using NameIDMap = std::map<std::string, uint32_t>;
using NameIDPair = std::pair<std::string, uint32_t>;

struct TransportSecurityStateEntry {
  TransportSecurityStateEntry();
  ~TransportSecurityStateEntry();

  std::string hostname;

  bool include_subdomains = false;
  bool force_https = false;

  bool hpkp_include_subdomains = false;
  std::string pinset;
};

using TransportSecurityStateEntries =
    std::vector<std::unique_ptr<TransportSecurityStateEntry>>;

class TransportSecurityStateTrieEntry : public huffman_trie::TrieEntry {
 public:
  TransportSecurityStateTrieEntry(const NameIDMap& pinsets_map,
                                  TransportSecurityStateEntry* entry);
  ~TransportSecurityStateTrieEntry() override;

  // huffman_trie::TrieEntry:
  std::string name() const override;
  bool WriteEntry(huffman_trie::TrieBitBuffer* writer) const override;

 private:
  const NameIDMap& pinsets_map_;
  TransportSecurityStateEntry* entry_;
};

}  // namespace net::transport_security_state

#endif  // NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_TRANSPORT_SECURITY_STATE_ENTRY_H_
