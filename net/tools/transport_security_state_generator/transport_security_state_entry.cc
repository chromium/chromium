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
  return entry->force_https && entry->include_subdomains &&
         entry->pinset.empty();
}

}  // namespace

TransportSecurityStateEntry::TransportSecurityStateEntry() = default;
TransportSecurityStateEntry::~TransportSecurityStateEntry() = default;

TransportSecurityStateTrieEntry::TransportSecurityStateTrieEntry(
    const NameIDMap& pinsets_map,
    TransportSecurityStateEntry* entry)
    : pinsets_map_(pinsets_map), entry_(entry) {}

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

  if (entry_->pinset.size()) {
    writer->WriteBit(1);

    auto pin_id_it = pinsets_map_.find(entry_->pinset);
    if (pin_id_it == pinsets_map_.cend()) {
      return false;
    }

    const uint8_t& pin_id = pin_id_it->second;
    if (pin_id > 15) {
      return false;
    }

    writer->WriteBits(pin_id, 4);

    if (!entry_->include_subdomains) {
      uint8_t include_subdomains_for_pinning = 0;
      if (entry_->hpkp_include_subdomains) {
        include_subdomains_for_pinning = 1;
      }
      writer->WriteBit(include_subdomains_for_pinning);
    }
  } else {
    writer->WriteBit(0);
  }

  return true;
}

}  // namespace net::transport_security_state
