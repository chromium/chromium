// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_PARTITION_KEY_COLLECTION_H_
#define NET_COOKIES_COOKIE_PARTITION_KEY_COLLECTION_H_

#include <iosfwd>
#include <optional>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "net/base/net_export.h"
#include "net/cookies/cookie_partition_key.h"

namespace net {

// A data structure used to represent a collection of cookie partition keys.
//
// It can represent all possible cookie partition keys when
// `ContainsAllKeys()` is true.
//
// It can also represent a finite number of cookie partition keys, including
// zero.
class NET_EXPORT CookiePartitionKeyCollection {
 public:
  // Creates an empty key collection.
  CookiePartitionKeyCollection();
  // Creates a key collection with a single element.
  explicit CookiePartitionKeyCollection(CookiePartitionKey key);
  // Creates a set that contains each partition key in the set.
  explicit CookiePartitionKeyCollection(
      base::flat_set<CookiePartitionKey> keys);

  explicit CookiePartitionKeyCollection(
      std::optional<CookiePartitionKey> opt_key);

  CookiePartitionKeyCollection(const CookiePartitionKeyCollection& other);
  CookiePartitionKeyCollection(CookiePartitionKeyCollection&& other);
  CookiePartitionKeyCollection& operator=(
      const CookiePartitionKeyCollection& other);
  CookiePartitionKeyCollection& operator=(CookiePartitionKeyCollection&& other);

  ~CookiePartitionKeyCollection();

  static CookiePartitionKeyCollection ContainsAll() {
    return CookiePartitionKeyCollection(PrivateTag{}, InternalState());
  }

  // Builds a Collection that contains the same-site and cross-site
  // partitionKeys associated with the `top_level_site`.
  // `top_level_site` must be non-empty and valid.
  static CookiePartitionKeyCollection MatchesSite(
      const net::SchemefulSite& top_level_site);


  // Temporary method used to record where we need to decide how to build the
  // CookiePartitionKeyCollection.
  //
  // Returns an empty key collection, so no partitioned cookies will be returned
  // at callsites this is used.
  //
  // TODO(crbug.com/40188414): Remove this method and update callsites to use
  // appropriate constructor.
  static CookiePartitionKeyCollection Todo() {
    return CookiePartitionKeyCollection();
  }

  // CookieMonster can check if the key collection is empty to avoid searching
  // the PartitionedCookieMap at all.
  bool IsEmpty() const { return state_ && state_->empty(); }

  // Returns if the key collection contains every partition key.
  bool ContainsAllKeys() const { return !state_; }

  // Iterate over all keys in the key collection, do not call this method if
  // `ContainsAllKeys()` is true.
  const base::flat_set<CookiePartitionKey>& PartitionKeys() const {
    CHECK(!ContainsAllKeys())
        << "Do not call PartitionKeys when ContainsAllKeys is true";
    return state_.value();
  }

  // Returns true if the collection contains the passed key.
  bool Contains(const CookiePartitionKey& key) const;

  friend bool operator==(const CookiePartitionKeyCollection& lhs,
                         const CookiePartitionKeyCollection& rhs) = default;

 private:
  using InternalState = std::optional<base::flat_set<CookiePartitionKey>>;
  // Used to disambiguate the ctors that accept std::optional values, since
  // usage of std::nullopt would be ambiguous otherwise.
  struct PrivateTag {};

  explicit CookiePartitionKeyCollection(PrivateTag, InternalState state);

  // If this is nullopt, the instance matches all keys. Otherwise, it matches
  // exactly the keys in `state_.value()`.
  InternalState state_;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const CookiePartitionKeyCollection& keys);

}  // namespace net

#endif  // NET_COOKIES_COOKIE_PARTITION_KEY_COLLECTION_H_
