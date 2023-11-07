// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HASH_VALUE_H_
#define NET_BASE_HASH_VALUE_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "net/base/net_export.h"

namespace net {

struct NET_EXPORT SHA256HashValue {
  unsigned char data[32];
};

inline bool operator==(const SHA256HashValue& lhs, const SHA256HashValue& rhs) {
  return memcmp(lhs.data, rhs.data, sizeof(lhs.data)) == 0;
}

inline bool operator!=(const SHA256HashValue& lhs, const SHA256HashValue& rhs) {
  return memcmp(lhs.data, rhs.data, sizeof(lhs.data)) != 0;
}

inline bool operator<(const SHA256HashValue& lhs, const SHA256HashValue& rhs) {
  return memcmp(lhs.data, rhs.data, sizeof(lhs.data)) < 0;
}

inline bool operator>(const SHA256HashValue& lhs, const SHA256HashValue& rhs) {
  return memcmp(lhs.data, rhs.data, sizeof(lhs.data)) > 0;
}

inline bool operator<=(const SHA256HashValue& lhs, const SHA256HashValue& rhs) {
  return memcmp(lhs.data, rhs.data, sizeof(lhs.data)) <= 0;
}

inline bool operator>=(const SHA256HashValue& lhs, const SHA256HashValue& rhs) {
  return memcmp(lhs.data, rhs.data, sizeof(lhs.data)) >= 0;
}

enum HashValueTag {
  HASH_VALUE_SHA256,
};

class NET_EXPORT HashValue {
 public:
  explicit HashValue(const SHA256HashValue& hash);
  explicit HashValue(HashValueTag tag) : tag_(tag) {}
  HashValue() : tag_(HASH_VALUE_SHA256) {}

  // Serializes/Deserializes hashes in the form of
  // <hash-name>"/"<base64-hash-value>
  // (eg: "sha256/...")
  // This format may be persisted to permanent storage, so
  // care should be taken before changing the serialization.
  //
  // This format is used for:
  //   - net_internals display/setting public-key pins
  //   - logging public-key pins
  //   - serializing public-key pins

  // Deserializes a HashValue from a string. Returns false if the input is not
  // valid.
  bool FromString(std::string_view input);

  // Serializes the HashValue to a string.
  std::string ToString() const;

  size_t size() const;
  unsigned char* data();
  const unsigned char* data() const;

  HashValueTag tag() const { return tag_; }

  NET_EXPORT friend bool operator==(const HashValue& lhs, const HashValue& rhs);
  NET_EXPORT friend bool operator!=(const HashValue& lhs, const HashValue& rhs);
  NET_EXPORT friend bool operator<(const HashValue& lhs, const HashValue& rhs);
  NET_EXPORT friend bool operator>(const HashValue& lhs, const HashValue& rhs);
  NET_EXPORT friend bool operator<=(const HashValue& lhs, const HashValue& rhs);
  NET_EXPORT friend bool operator>=(const HashValue& lhs, const HashValue& rhs);

 private:
  HashValueTag tag_;

  union {
    SHA256HashValue sha256;
  } fingerprint;
};

typedef std::vector<HashValue> HashValueVector;


// IsSHA256HashInSortedArray returns true iff |hash| is in |array|, a sorted
// array of SHA256 hashes.
bool IsSHA256HashInSortedArray(const HashValue& hash,
                               base::span<const SHA256HashValue> array);

// IsAnySHA256HashInSortedArray returns true iff any value in |hashes| is in
// |array|, a sorted array of SHA256 hashes.
bool IsAnySHA256HashInSortedArray(base::span<const HashValue> hashes,
                                  base::span<const SHA256HashValue> array);

}  // namespace net

#endif  // NET_BASE_HASH_VALUE_H_
