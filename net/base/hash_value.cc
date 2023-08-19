// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/hash_value.h"

#include <stdlib.h>

#include <algorithm>
#include <ostream>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "crypto/sha2.h"

namespace net {

namespace {

constexpr base::StringPiece kSha256Slash = "sha256/";

// LessThan comparator for use with std::binary_search() in determining
// whether a SHA-256 HashValue appears within a sorted array of
// SHA256HashValues.
struct SHA256ToHashValueComparator {
  bool operator()(const SHA256HashValue& lhs, const HashValue& rhs) const {
    DCHECK_EQ(HASH_VALUE_SHA256, rhs.tag());
    return memcmp(lhs.data, rhs.data(), rhs.size()) < 0;
  }

  bool operator()(const HashValue& lhs, const SHA256HashValue& rhs) const {
    DCHECK_EQ(HASH_VALUE_SHA256, lhs.tag());
    return memcmp(lhs.data(), rhs.data, lhs.size()) < 0;
  }
};

}  // namespace


HashValue::HashValue(const SHA256HashValue& hash)
    : HashValue(HASH_VALUE_SHA256) {
  fingerprint.sha256 = hash;
}

bool HashValue::FromString(base::StringPiece value) {
  if (!base::StartsWith(value, kSha256Slash)) {
    return false;
  }

  base::StringPiece base64_str = value.substr(kSha256Slash.size());

  auto decoded = base::Base64Decode(base64_str);
  if (!decoded || decoded->size() != size()) {
    return false;
  }
  tag_ = HASH_VALUE_SHA256;
  memcpy(data(), decoded->data(), size());
  return true;
}

std::string HashValue::ToString() const {
  std::string base64_str = base::Base64Encode(base::make_span(data(), size()));
  switch (tag_) {
    case HASH_VALUE_SHA256:
      return std::string(kSha256Slash) + base64_str;
  }

  NOTREACHED_NORETURN();
}

size_t HashValue::size() const {
  switch (tag_) {
    case HASH_VALUE_SHA256:
      return sizeof(fingerprint.sha256.data);
  }

  NOTREACHED_NORETURN();
}

unsigned char* HashValue::data() {
  return const_cast<unsigned char*>(const_cast<const HashValue*>(this)->data());
}

const unsigned char* HashValue::data() const {
  switch (tag_) {
    case HASH_VALUE_SHA256:
      return fingerprint.sha256.data;
  }

  NOTREACHED_NORETURN();
}

bool operator==(const HashValue& lhs, const HashValue& rhs) {
  if (lhs.tag_ != rhs.tag_)
    return false;

  switch (lhs.tag_) {
    case HASH_VALUE_SHA256:
      return lhs.fingerprint.sha256 == rhs.fingerprint.sha256;
  }

  NOTREACHED_NORETURN();
}

bool operator!=(const HashValue& lhs, const HashValue& rhs) {
  return !(lhs == rhs);
}

bool operator<(const HashValue& lhs, const HashValue& rhs) {
  if (lhs.tag_ != rhs.tag_)
    return lhs.tag_ < rhs.tag_;

  switch (lhs.tag_) {
    case HASH_VALUE_SHA256:
      return lhs.fingerprint.sha256 < rhs.fingerprint.sha256;
  }

  NOTREACHED();
  return false;
}

bool operator>(const HashValue& lhs, const HashValue& rhs) {
  return rhs < lhs;
}

bool operator<=(const HashValue& lhs, const HashValue& rhs) {
  return !(lhs > rhs);
}

bool operator>=(const HashValue& lhs, const HashValue& rhs) {
  return !(lhs < rhs);
}

bool IsSHA256HashInSortedArray(const HashValue& hash,
                               base::span<const SHA256HashValue> array) {
  return std::binary_search(array.begin(), array.end(), hash,
                            SHA256ToHashValueComparator());
}

bool IsAnySHA256HashInSortedArray(base::span<const HashValue> hashes,
                                  base::span<const SHA256HashValue> array) {
  for (const auto& hash : hashes) {
    if (hash.tag() != HASH_VALUE_SHA256)
      continue;

    if (IsSHA256HashInSortedArray(hash, array))
      return true;
  }
  return false;
}

}  // namespace net
