// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_partition_key_collection.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_access_delegate.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/first_party_sets/first_party_set_entry.h"

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

bool CookiePartitionKeyCollection::Contains(
    const CookiePartitionKey& key) const {
  return contains_all_keys_ || base::Contains(keys_, key);
}

bool operator==(const CookiePartitionKeyCollection& lhs,
                const CookiePartitionKeyCollection& rhs) {
  if (lhs.ContainsAllKeys()) {
    return rhs.ContainsAllKeys();
  }

  if (rhs.ContainsAllKeys()) {
    return false;
  }

  return lhs.PartitionKeys() == rhs.PartitionKeys();
}

CookiePartitionKeyCollection CookiePartitionKeyCollection::MatchesSite(
    const net::SchemefulSite& top_level_site) {
  base::expected<net::CookiePartitionKey, std::string> same_site_key =
      CookiePartitionKey::FromWire(
          top_level_site, CookiePartitionKey::AncestorChainBit::kSameSite);
  base::expected<net::CookiePartitionKey, std::string> cross_site_key =
      CookiePartitionKey::FromWire(
          top_level_site, CookiePartitionKey::AncestorChainBit::kCrossSite);

  CHECK(cross_site_key.has_value());
  CHECK(same_site_key.has_value());

  return net::CookiePartitionKeyCollection(
      {same_site_key.value(), cross_site_key.value()});
}

std::ostream& operator<<(std::ostream& os,
                         const CookiePartitionKeyCollection& keys) {
  if (keys.ContainsAllKeys()) {
    return os << "(all keys)";
  }

  os << "{";
  bool first = true;
  for (const net::CookiePartitionKey& key : keys.PartitionKeys()) {
    if (!first) {
      os << ", ";
    }

    os << key;

    first = false;
  }
  return os << "}";
}

}  // namespace net
