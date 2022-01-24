// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_partition_key_collection.h"

#include <vector>

#include "base/barrier_callback.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "net/cookies/cookie_access_delegate.h"
#include "net/cookies/cookie_partition_key.h"

namespace net {

CookiePartitionKeyCollection::CookiePartitionKeyCollection() = default;

CookiePartitionKeyCollection::CookiePartitionKeyCollection(
    const CookiePartitionKeyCollection& other) = default;

CookiePartitionKeyCollection::CookiePartitionKeyCollection(
    CookiePartitionKeyCollection&& other) = default;

CookiePartitionKeyCollection::CookiePartitionKeyCollection(
    const CookiePartitionKey& key)
    : CookiePartitionKeyCollection(base::flat_set<CookiePartitionKey>({key})) {}

CookiePartitionKeyCollection::CookiePartitionKeyCollection(
    base::flat_set<CookiePartitionKey> keys)
    : keys_(std::move(keys)) {}

CookiePartitionKeyCollection::CookiePartitionKeyCollection(
    bool contains_all_keys)
    : contains_all_keys_(contains_all_keys) {}

CookiePartitionKeyCollection& CookiePartitionKeyCollection::operator=(
    const CookiePartitionKeyCollection& other) = default;

CookiePartitionKeyCollection& CookiePartitionKeyCollection::operator=(
    CookiePartitionKeyCollection&& other) = default;

CookiePartitionKeyCollection::~CookiePartitionKeyCollection() = default;

void CookiePartitionKeyCollection::FirstPartySetify(
    const CookieAccessDelegate* cookie_access_delegate,
    base::OnceCallback<void(CookiePartitionKeyCollection)> callback) const {
  if (!cookie_access_delegate || IsEmpty() || ContainsAllKeys()) {
    std::move(callback).Run(*this);
    return;
  }

  // TODO(cfredric): rewrite this to make a single round trip (batched) instead
  // of 1 trip for each key.
  auto barrier = base::BarrierCallback<
      std::pair<net::CookiePartitionKey, absl::optional<net::SchemefulSite>>>(
      PartitionKeys().size(),
      base::BindOnce(
          [](base::OnceCallback<void(CookiePartitionKeyCollection)> callback,
             const std::vector<std::pair<net::CookiePartitionKey,
                                         absl::optional<net::SchemefulSite>>>&
                 keys_and_owners) {
            std::vector<net::CookiePartitionKey> keys;
            keys.reserve(keys_and_owners.size());
            for (const auto& key_and_owner : keys_and_owners) {
              const net::CookiePartitionKey& key = key_and_owner.first;
              const absl::optional<SchemefulSite>& first_party_set_owner =
                  key_and_owner.second;
              keys.push_back(
                  first_party_set_owner.has_value()
                      ? CookiePartitionKey::FromWire(
                            first_party_set_owner.value(), key.nonce())
                      : key);
            }
            std::move(callback).Run(CookiePartitionKeyCollection(keys));
          },
          std::move(callback)));

  for (const auto& key : PartitionKeys()) {
    cookie_access_delegate->FindFirstPartySetOwner(
        key.site(),
        base::BindOnce(
            [](base::RepeatingCallback<void(
                   std::pair<net::CookiePartitionKey,
                             absl::optional<net::SchemefulSite>>)> barrier,
               CookiePartitionKey key,
               absl::optional<net::SchemefulSite> owner) {
              barrier.Run(std::make_pair(key, owner));
            },
            barrier, key));
  }
}

bool CookiePartitionKeyCollection::Contains(
    const CookiePartitionKey& key) const {
  return contains_all_keys_ || base::Contains(keys_, key);
}

}  // namespace net
