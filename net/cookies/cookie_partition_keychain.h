// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_PARTITION_KEYCHAIN_H_
#define NET_COOKIES_COOKIE_PARTITION_KEYCHAIN_H_

#include <set>
#include <vector>

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
// TODO(crbug.com/1225444): Consider changing the name of this class since the
// term "keychain" has a certain meaning for iOS and macOS.
class NET_EXPORT CookiePartitionKeychain {
 public:
  // Creates an empty keychain.
  explicit CookiePartitionKeychain();
  CookiePartitionKeychain(const CookiePartitionKeychain& other);
  CookiePartitionKeychain(CookiePartitionKeychain&& other);
  // Creates a keychain with a single element.
  explicit CookiePartitionKeychain(const CookiePartitionKey& key);
  // Creates a set that contains each partition key in the vector.
  explicit CookiePartitionKeychain(const std::vector<CookiePartitionKey>& keys);

  CookiePartitionKeychain& operator=(const CookiePartitionKeychain& other);
  CookiePartitionKeychain& operator=(CookiePartitionKeychain&& other);
  ~CookiePartitionKeychain();

  static CookiePartitionKeychain ContainsAll() {
    return CookiePartitionKeychain(true);
  }

  static CookiePartitionKeychain FromOptional(
      const absl::optional<CookiePartitionKey>& opt_key) {
    return opt_key ? CookiePartitionKeychain(opt_key.value())
                   : CookiePartitionKeychain();
  }

  // Temporary method used to record where we need to decide how to build the
  // CookiePartitionKeychain.
  //
  // Returns an empty keychain, so no partitioned cookies will be returned at
  // callsites this is used.
  //
  // TODO(crbug.com/1225444): Remove this method and update callsites to use
  // appropriate constructor.
  static CookiePartitionKeychain Todo() { return CookiePartitionKeychain(); }

  // CookieMonster can check if the keychain is empty to avoid searching the
  // PartitionedCookieMap at all.
  bool IsEmpty() const { return !contains_all_keys_ && keys_.empty(); }

  // Returns if the keychain contains every partition key.
  bool ContainsAllKeys() const { return contains_all_keys_; }

  // Iterate over all keys in the keychain, do not call this method if
  // `contains_all_keys` is true.
  const std::vector<CookiePartitionKey>& PartitionKeys() const {
    DCHECK(!contains_all_keys_);
    return keys_;
  }

 private:
  explicit CookiePartitionKeychain(bool contains_all_keys_);

  bool contains_all_keys_ = false;
  // If `contains_all_keys_` is true, `keys_` must be empty.
  // If `keys_` is not empty, then `contains_all_keys_` must be false.
  std::vector<CookiePartitionKey> keys_;
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_PARTITION_KEYCHAIN_H_
