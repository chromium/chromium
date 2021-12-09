// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_partition_key_collection.h"

namespace net {

CookiePartitionKeyCollection::CookiePartitionKeyCollection() = default;

CookiePartitionKeyCollection::CookiePartitionKeyCollection(
    const CookiePartitionKeyCollection& other) = default;

CookiePartitionKeyCollection::CookiePartitionKeyCollection(
    CookiePartitionKeyCollection&& other) = default;

CookiePartitionKeyCollection::CookiePartitionKeyCollection(
    const CookiePartitionKey& key) {
  keys_.push_back(key);
}

CookiePartitionKeyCollection::CookiePartitionKeyCollection(
    const std::vector<CookiePartitionKey>& keys)
    : keys_(keys) {}

CookiePartitionKeyCollection::CookiePartitionKeyCollection(
    bool contains_all_keys_)
    : contains_all_keys_(contains_all_keys_) {}

CookiePartitionKeyCollection& CookiePartitionKeyCollection::operator=(
    const CookiePartitionKeyCollection& other) = default;

CookiePartitionKeyCollection& CookiePartitionKeyCollection::operator=(
    CookiePartitionKeyCollection&& other) = default;

CookiePartitionKeyCollection::~CookiePartitionKeyCollection() = default;

CookiePartitionKeyCollection CookiePartitionKeyCollection::FirstPartySetify(
    const CookieAccessDelegate* cookie_access_delegate) const {
  if (!cookie_access_delegate || IsEmpty() || ContainsAllKeys())
    return *this;
  std::vector<CookiePartitionKey> keys;
  keys.reserve(PartitionKeys().size());
  for (const auto& key : PartitionKeys()) {
    absl::optional<SchemefulSite> fps_owner =
        cookie_access_delegate->FindFirstPartySetOwner(key.site());
    if (fps_owner) {
      keys.push_back(
          CookiePartitionKey::FromWire(fps_owner.value(), key.nonce()));
    } else {
      keys.push_back(key);
    }
  }
  return CookiePartitionKeyCollection(keys);
}

}  // namespace net
