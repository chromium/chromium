// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/esni_content.h"

namespace net {

EsniContent::EsniContent() = default;
EsniContent::EsniContent(const EsniContent& other) {
  MergeFrom(other);
}
EsniContent::EsniContent(EsniContent&& other) = default;
EsniContent& EsniContent::operator=(const EsniContent& other) {
  MergeFrom(other);
  return *this;
}
EsniContent& EsniContent::operator=(EsniContent&& other) = default;
EsniContent::~EsniContent() = default;

bool operator==(const EsniContent& c1, const EsniContent& c2) {
  return c1.keys() == c2.keys() &&
         c1.keys_for_addresses() == c2.keys_for_addresses();
}

const std::set<std::string, EsniContent::StringPieceComparator>&
EsniContent::keys() const {
  return keys_;
}

const std::map<IPAddress, std::set<base::StringPiece>>&
EsniContent::keys_for_addresses() const {
  return keys_for_addresses_;
}

void EsniContent::AddKey(base::StringPiece key) {
  if (keys_.find(key) == keys_.end())
    keys_.insert(std::string(key));
}

void EsniContent::AddKeyForAddress(const IPAddress& address,
                                   base::StringPiece key) {
  auto key_it = keys_.find(key);
  if (key_it == keys_.end()) {
    bool key_was_added;
    std::tie(key_it, key_was_added) = keys_.insert(std::string(key));
    DCHECK(key_was_added);
  }
  keys_for_addresses_[address].insert(base::StringPiece(*key_it));
}

void EsniContent::MergeFrom(const EsniContent& other) {
  for (const auto& kv : other.keys_for_addresses()) {
    const IPAddress& address = kv.first;
    const auto& keys_for_address = kv.second;
    for (base::StringPiece key : keys_for_address)
      AddKeyForAddress(address, key);
  }
  for (const std::string& key : other.keys())
    AddKey(key);
}

}  // namespace net
