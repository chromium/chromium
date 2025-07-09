// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/pseudonymization_util.h"

#include <string_view>

#include "base/numerics/byte_conversions.h"
#include "content/common/pseudonymization_salt.h"
#include "crypto/hash.h"

namespace content {

// static
uint32_t PseudonymizationUtil::PseudonymizeStringForTesting(
    std::string_view string) {
  return PseudonymizeString(string);
}

// static
uint32_t PseudonymizationUtil::PseudonymizeString(std::string_view string) {
  crypto::hash::Hasher hash(crypto::hash::kSha256);
  hash.Update(string);

  // When `string` comes from a small set of possible strings (or when it is
  // possible to compare a hash with results of hashing the 100 most common
  // input strings), then its hash can be deanonymized.  To protect against this
  // threat, we include a random `salt` in the SHA-256 hash (the salt is never
  // retained or sent anywhere).
  uint32_t salt = GetPseudonymizationSalt();
  hash.Update(base::byte_span_from_ref(salt));

  std::array<uint8_t, crypto::hash::kSha256Size> result;
  hash.Finish(result);

  // Taking just the first 4 bytes is okay, because the entropy of the input
  // string is uniformly distributed across the entire hash.
  return base::U32FromNativeEndian(
      base::span(result).first<sizeof(uint32_t)>());
}

}  // namespace content
