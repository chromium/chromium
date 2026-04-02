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

struct TransportSecurityStateEntry {
  TransportSecurityStateEntry();
  ~TransportSecurityStateEntry();

  std::string hostname;

  bool include_subdomains = false;
  bool force_https = false;
};

using TransportSecurityStateEntries =
    std::vector<std::unique_ptr<TransportSecurityStateEntry>>;

class TransportSecurityStateTrieEntry : public huffman_trie::TrieEntry {
 public:
  explicit TransportSecurityStateTrieEntry(TransportSecurityStateEntry* entry);
  ~TransportSecurityStateTrieEntry() override;

  // huffman_trie::TrieEntry:
  std::string name() const override;
  bool WriteEntry(huffman_trie::TrieBitBuffer* writer) const override;

 private:
  TransportSecurityStateEntry* entry_;
};

struct PinEntry {
  std::string hostname;
  std::string pinset;
  bool include_subdomains = false;
};

using PinEntries = std::vector<std::unique_ptr<PinEntry>>;

}  // namespace net::transport_security_state

#endif  // NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_TRANSPORT_SECURITY_STATE_ENTRY_H_
