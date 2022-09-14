// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_partition_key_collection.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_access_delegate.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/first_party_sets/first_party_set_entry.h"

namespace net {

namespace {
CookiePartitionKeyCollection TransformWithFirstPartySetEntries(
    const base::flat_set<CookiePartitionKey>& keys,
    base::flat_map<SchemefulSite, FirstPartySetEntry> sites_to_entries) {
  std::vector<CookiePartitionKey> canonicalized_keys;
  canonicalized_keys.reserve(keys.size());
  for (const CookiePartitionKey& key : keys) {
    const auto it = sites_to_entries.find(key.site());
    canonicalized_keys.push_back(
        !key.nonce() && it != sites_to_entries.end()
            ? CookiePartitionKey::FromWire(it->second.primary())
            : key);
  }
  return CookiePartitionKeyCollection(canonicalized_keys);
}
}  // namespace

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

absl::optional<CookiePartitionKeyCollection>
CookiePartitionKeyCollection::FirstPartySetify(
    const CookieAccessDelegate* cookie_access_delegate,
    base::OnceCallback<void(CookiePartitionKeyCollection)> callback) const {
  if (!cookie_access_delegate || IsEmpty() || ContainsAllKeys())
    return *this;

  std::vector<SchemefulSite> sites;
  sites.reserve(PartitionKeys().size());
  for (const CookiePartitionKey& key : PartitionKeys()) {
    // Partition keys that have a nonce are not available across top-level sites
    // in the same First-Party Set.
    if (key.nonce())
      continue;
    sites.push_back(key.site());
  }
  if (sites.empty())
    return *this;
  absl::optional<base::flat_map<SchemefulSite, FirstPartySetEntry>>
      maybe_sites_to_entries = cookie_access_delegate->FindFirstPartySetEntries(
          sites,
          base::BindOnce(&TransformWithFirstPartySetEntries, PartitionKeys())
              .Then(std::move(callback)));

  if (maybe_sites_to_entries.has_value())
    return TransformWithFirstPartySetEntries(PartitionKeys(),
                                             maybe_sites_to_entries.value());

  return absl::nullopt;
}

bool CookiePartitionKeyCollection::Contains(
    const CookiePartitionKey& key) const {
  return contains_all_keys_ || base::Contains(keys_, key);
}

}  // namespace net
