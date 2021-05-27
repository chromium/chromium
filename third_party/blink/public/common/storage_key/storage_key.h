// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_H_

#include <string>

#include "third_party/blink/public/common/common_export.h"
#include "url/origin.h"

namespace blink {

// A class representing the key that Storage APIs use to key their storage on.
class BLINK_COMMON_EXPORT StorageKey {
 public:
  StorageKey() = default;
  explicit StorageKey(const url::Origin& origin) : origin_(origin) {}

  // Copyable and Moveable.
  StorageKey(const StorageKey& other) = default;
  StorageKey& operator=(const StorageKey& other) = default;
  StorageKey(StorageKey&& other) = default;
  StorageKey& operator=(StorageKey&& other) = default;

  ~StorageKey() = default;

  // Returns a newly constructed StorageKey from, a previously serialized, `in`.
  // If `in` is invalid then the StorageKey will be opaque. A deserialized
  // StorageKey will be equivalent to the StorageKey that was initially
  // serialized.
  static StorageKey Deserialize(const std::string& in);

  // Serializes the `StorageKey` into a string.
  // This function will return the spec url of the underlying Origin. Do not
  // call if `this` is opaque.
  std::string Serialize() const;

  bool opaque() const { return origin_.opaque(); }

  const url::Origin& origin() const { return origin_; }

 private:
  BLINK_COMMON_EXPORT
  friend bool operator==(const StorageKey& lhs, const StorageKey& rhs);

  BLINK_COMMON_EXPORT
  friend bool operator!=(const StorageKey& lhs, const StorageKey& rhs);

  // Allows StorageKey to be used as a key in STL (for example, a std::set or
  // std::map).
  BLINK_COMMON_EXPORT
  friend bool operator<(const StorageKey& lhs, const StorageKey& rhs);

  url::Origin origin_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_H_
