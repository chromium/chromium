// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_H_

#include <iosfwd>
#include <string>

#include "base/strings/string_piece.h"
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "url/origin.h"

namespace blink {

// A class representing the key that Storage APIs use to key their storage on.
//
// At the moment, StorageKey contains an origin and an optional nonce. The
// top-frame site will be added later in order to implement a finer storage
// partitioning. Using the nonce is still unsupported since serialization and
// deserialization don't take it into account. For more details on the overall
// design, see
// https://docs.google.com/document/d/1xd6MXcUhfnZqIe5dt2CTyCn6gEZ7nOezAEWS0W9hwbQ/edit.
class BLINK_COMMON_EXPORT StorageKey {
 public:
  StorageKey() = default;
  explicit StorageKey(const url::Origin& origin)
      : StorageKey(origin, nullptr) {}

  static StorageKey CreateWithNonce(const url::Origin& origin,
                                    const base::UnguessableToken& nonce);

  // Copyable and Moveable.
  StorageKey(const StorageKey& other) = default;
  StorageKey& operator=(const StorageKey& other) = default;
  StorageKey(StorageKey&& other) = default;
  StorageKey& operator=(StorageKey&& other) = default;

  ~StorageKey() = default;

  // Returns a newly constructed StorageKey from, a previously serialized, `in`.
  // If `in` is invalid then the return value will be nullopt. If this returns a
  // non-nullopt value, it will be a valid, non-opaque StorageKey. A
  // deserialized StorageKey will be equivalent to the StorageKey that was
  // initially serialized.
  //
  // Can be called on the output of either Serialize() or
  // SerializeForLocalStorage(), as it can handle both formats.
  static absl::optional<StorageKey> Deserialize(base::StringPiece in);

  // Transforms a string into a StorageKey if possible (and an opaque StorageKey
  // if not). Currently calls Deserialize, but this may change in future.
  // For use in tests only.
  static StorageKey CreateFromStringForTesting(const std::string& origin);

  // Returns true if ThirdPartyStoragePartitioning feature flag is enabled.
  static bool IsThirdPartyStoragePartitioningEnabled();

  // Serializes the `StorageKey` into a string.
  // This function will return the spec url of the underlying Origin. Do not
  // call if `this` is opaque.
  std::string Serialize() const;

  // Serializes into a string in the format used for localStorage (without
  // trailing slashes). Prefer Serialize() for uses other than localStorage. Do
  // not call if `this` is opaque.
  std::string SerializeForLocalStorage() const;

  const url::Origin& origin() const { return origin_; }

  const absl::optional<base::UnguessableToken>& nonce() const { return nonce_; }

  std::string GetDebugString() const;

  // Provides a concise string representation suitable for memory dumps.
  // Limits the length to `max_length` chars and strips special characters.
  std::string GetMemoryDumpString(size_t max_length) const;

 private:
  StorageKey(const url::Origin& origin, const base::UnguessableToken* nonce)
      : origin_(origin),
        nonce_(nonce ? absl::make_optional(*nonce) : absl::nullopt) {}

  BLINK_COMMON_EXPORT
  friend bool operator==(const StorageKey& lhs, const StorageKey& rhs);

  BLINK_COMMON_EXPORT
  friend bool operator!=(const StorageKey& lhs, const StorageKey& rhs);

  // Allows StorageKey to be used as a key in STL (for example, a std::set or
  // std::map).
  BLINK_COMMON_EXPORT
  friend bool operator<(const StorageKey& lhs, const StorageKey& rhs);

  url::Origin origin_;

  // An optional nonce, forcing a partitioned storage from anything else. Used
  // by anonymous iframes:
  // https://github.com/camillelamy/explainers/blob/master/anonymous_iframes.md
  absl::optional<base::UnguessableToken> nonce_;
};

BLINK_COMMON_EXPORT
std::ostream& operator<<(std::ostream& ostream, const StorageKey& sk);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_STORAGE_KEY_STORAGE_KEY_H_
