// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_partition_key_collection.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
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

  std::vector<SchemefulSite> sites;
  sites.reserve(PartitionKeys().size());
  for (const CookiePartitionKey& key : PartitionKeys()) {
    sites.push_back(key.site());
  }
  cookie_access_delegate->FindFirstPartySetOwners(
      sites,
      base::BindOnce(
          [](const base::flat_set<CookiePartitionKey>& keys,
             base::OnceCallback<void(CookiePartitionKeyCollection)> callback,
             base::flat_map<SchemefulSite, SchemefulSite> sites_to_owners) {
            std::vector<CookiePartitionKey> canonicalized_keys;
            canonicalized_keys.reserve(keys.size());
            for (const CookiePartitionKey& key : keys) {
              const auto first_party_set_owner_iter =
                  sites_to_owners.find(key.site());
              canonicalized_keys.push_back(
                  first_party_set_owner_iter != sites_to_owners.end()
                      ? CookiePartitionKey::FromWire(
                            first_party_set_owner_iter->second, key.nonce())
                      : key);
            }
            std::move(callback).Run(
                CookiePartitionKeyCollection(canonicalized_keys));
          },
          PartitionKeys(), std::move(callback)));
}

bool CookiePartitionKeyCollection::Contains(
    const CookiePartitionKey& key) const {
  return contains_all_keys_ || base::Contains(keys_, key);
}

}  // namespace net
