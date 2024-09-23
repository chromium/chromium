// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_PARTITION_KEY_COLLECTION_H_
#define NET_COOKIES_COOKIE_PARTITION_KEY_COLLECTION_H_

#include <iosfwd>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "net/base/net_export.h"
#include "net/cookies/cookie_partition_key.h"

namespace net {

// A data structure used to represent a collection of cookie partition keys.
//
// It can represent all possible cookie partition keys when
// `contains_all_keys_` is true.
//
// It can also represent a finite number of cookie partition keys, including
// zero.
class NET_EXPORT CookiePartitionKeyCollection {
 public:
  // Creates an empty key collection.
  CookiePartitionKeyCollection();
  CookiePartitionKeyCollection(const CookiePartitionKeyCollection& other);
  CookiePartitionKeyCollection(CookiePartitionKeyCollection&& other);
  // Creates a key collection with a single element.
  explicit CookiePartitionKeyCollection(const CookiePartitionKey& key);
  // Creates a set that contains each partition key in the set.
  explicit CookiePartitionKeyCollection(
      base::flat_set<CookiePartitionKey> keys);

  CookiePartitionKeyCollection& operator=(
      const CookiePartitionKeyCollection& other);
  CookiePartitionKeyCollection& operator=(CookiePartitionKeyCollection&& other);
  ~CookiePartitionKeyCollection();

  static CookiePartitionKeyCollection ContainsAll() {
    return CookiePartitionKeyCollection(true);
  }

  // Builds a Collection that contains the same-site and cross-site
  // partitionKeys associated with the `top_level_site`.
  // `top_level_site` must be non-empty and valid.
  static CookiePartitionKeyCollection MatchesSite(
      const net::SchemefulSite& top_level_site);

  static CookiePartitionKeyCollection FromOptional(
      const std::optional<CookiePartitionKey>& opt_key) {
    return opt_key ? CookiePartitionKeyCollection(opt_key.value())
                   : CookiePartitionKeyCollection();
  }

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
  bool IsEmpty() const { return !contains_all_keys_ && keys_.empty(); }

  // Returns if the key collection contains every partition key.
  bool ContainsAllKeys() const { return contains_all_keys_; }

  // Iterate over all keys in the key collection, do not call this method if
  // `contains_all_keys` is true.
  const base::flat_set<CookiePartitionKey>& PartitionKeys() const {
    DCHECK(!contains_all_keys_);
    return keys_;
  }

  // Returns true if the collection contains the passed key.
  bool Contains(const CookiePartitionKey& key) const;

 private:
  explicit CookiePartitionKeyCollection(bool contains_all_keys);

  bool contains_all_keys_ = false;
  // If `contains_all_keys_` is true, `keys_` must be empty.
  // If `keys_` is not empty, then `contains_all_keys_` must be false.
  base::flat_set<CookiePartitionKey> keys_;
};

NET_EXPORT bool operator==(const CookiePartitionKeyCollection& lhs,
                           const CookiePartitionKeyCollection& rhs);

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const CookiePartitionKeyCollection& keys);

}  // namespace net

#endif  // NET_COOKIES_COOKIE_PARTITION_KEY_COLLECTION_H_
