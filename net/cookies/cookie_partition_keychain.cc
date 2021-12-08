// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_partition_keychain.h"

namespace net {

CookiePartitionKeychain::CookiePartitionKeychain() = default;

CookiePartitionKeychain::CookiePartitionKeychain(
    const CookiePartitionKeychain& other) = default;

CookiePartitionKeychain::CookiePartitionKeychain(
    CookiePartitionKeychain&& other) = default;

CookiePartitionKeychain::CookiePartitionKeychain(
    const CookiePartitionKey& key) {
  keys_.push_back(key);
}

CookiePartitionKeychain::CookiePartitionKeychain(
    const std::vector<CookiePartitionKey>& keys)
    : keys_(keys) {}

CookiePartitionKeychain::CookiePartitionKeychain(bool contains_all_keys_)
    : contains_all_keys_(contains_all_keys_) {}

CookiePartitionKeychain& CookiePartitionKeychain::operator=(
    const CookiePartitionKeychain& other) = default;

CookiePartitionKeychain& CookiePartitionKeychain::operator=(
    CookiePartitionKeychain&& other) = default;

CookiePartitionKeychain::~CookiePartitionKeychain() = default;

CookiePartitionKeychain CookiePartitionKeychain::FirstPartySetify(
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
  return CookiePartitionKeychain(keys);
}

}  // namespace net
