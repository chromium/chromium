// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/transport_security_state_generator/transport_security_state_entry.h"
#include "net/tools/huffman_trie/trie/trie_bit_buffer.h"

namespace net::transport_security_state {

namespace {

// Returns true if the entry only configures HSTS with includeSubdomains.
// Such entries, when written, can be represented more compactly, and thus
// reduce the overall size of the trie.
bool IsSimpleEntry(const TransportSecurityStateEntry* entry) {
  return entry->force_https && entry->include_subdomains;
}

}  // namespace

TransportSecurityStateEntry::TransportSecurityStateEntry() = default;
TransportSecurityStateEntry::~TransportSecurityStateEntry() = default;

TransportSecurityStateTrieEntry::TransportSecurityStateTrieEntry(
    TransportSecurityStateEntry* entry)
    : entry_(entry) {}

TransportSecurityStateTrieEntry::~TransportSecurityStateTrieEntry() = default;

std::string TransportSecurityStateTrieEntry::name() const {
  return entry_->hostname;
}

bool TransportSecurityStateTrieEntry::WriteEntry(
    huffman_trie::TrieBitBuffer* writer) const {
  if (IsSimpleEntry(entry_)) {
    writer->WriteBit(1);
    return true;
  } else {
    writer->WriteBit(0);
  }

  uint8_t include_subdomains = 0;
  if (entry_->include_subdomains) {
    include_subdomains = 1;
  }
  writer->WriteBit(include_subdomains);

  uint8_t force_https = 0;
  if (entry_->force_https) {
    force_https = 1;
  }
  writer->WriteBit(force_https);

  return true;
}

}  // namespace net::transport_security_state
