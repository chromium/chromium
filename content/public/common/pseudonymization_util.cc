// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/pseudonymization_util.h"

#include <string.h>

#include <string_view>

#include "base/hash/sha1.h"
#include "content/common/pseudonymization_salt.h"

namespace content {

// static
uint32_t PseudonymizationUtil::PseudonymizeStringForTesting(
    std::string_view string) {
  return PseudonymizeString(string);
}

// static
uint32_t PseudonymizationUtil::PseudonymizeString(std::string_view string) {
  // Include `string` in the SHA1 hash.
  base::SHA1Context sha1_context;
  base::SHA1Init(sha1_context);
  base::SHA1Update(string, sha1_context);

  // When `string` comes from a small set of possible strings (or when it is
  // possible to compare a hash with results of hashing the 100 most common
  // input strings), then its hash can be deanonymized.  To protect against this
  // threat, we include a random `salt` in the SHA1 hash (the salt is never
  // retained or sent anywhere).
  uint32_t salt = GetPseudonymizationSalt();
  base::SHA1Update(
      std::string_view(reinterpret_cast<const char*>(&salt), sizeof(salt)),
      sha1_context);

  // Compute the SHA1 hash.
  base::SHA1Digest sha1_hash_bytes;
  base::SHA1Final(sha1_context, sha1_hash_bytes);

  // Taking just the first 4 bytes is okay, because SHA1 should uniformly
  // distribute all possible results over all of the `sha1_hash_bytes`.
  uint32_t hash;
  static_assert(
      sizeof(hash) <
          sizeof(base::SHA1Digest::value_type) * sha1_hash_bytes.size(),
      "Is `memcpy` safely within the bounds of `hash` and `sha1_hash_bytes`?");
  memcpy(&hash, sha1_hash_bytes.data(), sizeof(hash));

  return hash;
}

}  // namespace content
