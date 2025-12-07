// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/hash_value.h"

#include <stdlib.h>

#include <algorithm>
#include <ostream>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "crypto/sha2.h"

namespace net {

namespace {

constexpr std::string_view kSha256Slash = "sha256/";

}  // namespace


HashValue::HashValue(const SHA256HashValue& hash)
    : HashValue(HASH_VALUE_SHA256) {
  fingerprint.sha256 = hash;
}

HashValue::HashValue(base::span<const uint8_t> hash)
    : HashValue(HASH_VALUE_SHA256) {
  base::span(fingerprint.sha256).copy_from(hash);
}

bool HashValue::FromString(std::string_view value) {
  if (!value.starts_with(kSha256Slash)) {
    return false;
  }

  std::string_view base64_str = value.substr(kSha256Slash.size());

  auto decoded = base::Base64Decode(base64_str);
  if (!decoded || decoded->size() != span().size()) {
    return false;
  }
  tag_ = HASH_VALUE_SHA256;
  span().copy_from(*decoded);
  return true;
}

std::string HashValue::ToString() const {
  std::string base64_str = base::Base64Encode(span());
  switch (tag_) {
    case HASH_VALUE_SHA256:
      return std::string(kSha256Slash) + base64_str;
  }

  NOTREACHED();
}

base::span<uint8_t> HashValue::span() {
  switch (tag_) {
    case HASH_VALUE_SHA256:
      return fingerprint.sha256;
  }

  NOTREACHED();
}

base::span<const uint8_t> HashValue::span() const {
  switch (tag_) {
    case HASH_VALUE_SHA256:
      return fingerprint.sha256;
  }

  NOTREACHED();
}

const SHA256HashValue& HashValue::sha256hashvalue() const {
  CHECK(tag_ == HASH_VALUE_SHA256);
  return fingerprint.sha256;
}

bool operator==(const HashValue& lhs, const HashValue& rhs) {
  if (lhs.tag_ != rhs.tag_)
    return false;

  switch (lhs.tag_) {
    case HASH_VALUE_SHA256:
      return lhs.fingerprint.sha256 == rhs.fingerprint.sha256;
  }

  NOTREACHED();
}

bool operator<(const HashValue& lhs, const HashValue& rhs) {
  if (lhs.tag_ != rhs.tag_)
    return lhs.tag_ < rhs.tag_;

  switch (lhs.tag_) {
    case HASH_VALUE_SHA256:
      return lhs.fingerprint.sha256 < rhs.fingerprint.sha256;
  }

  NOTREACHED();
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

}  // namespace net
