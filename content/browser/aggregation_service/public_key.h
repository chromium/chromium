// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_PUBLIC_KEY_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_PUBLIC_KEY_H_

#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"

namespace content {

// Contains all the data of a public key.
struct CONTENT_EXPORT PublicKey {
  PublicKey(std::string id, std::vector<uint8_t> key);
  PublicKey(const PublicKey& other);
  PublicKey& operator=(const PublicKey& other);
  PublicKey(PublicKey&& other);
  PublicKey& operator=(PublicKey&& other);
  ~PublicKey();

  // String identifying the key, controlled by the helper server.
  std::string id;

  // The key itself.
  std::vector<uint8_t> key;

  static constexpr size_t kMaxIdSize = 128;

  // The expected length (in bytes) of the key.
  static constexpr size_t kKeyByteLength = X25519_PUBLIC_VALUE_LEN;
};

struct CONTENT_EXPORT PublicKeyset {
  PublicKeyset(std::vector<PublicKey> keys,
               base::Time fetch_time,
               base::Time expiry_time);
  PublicKeyset(const PublicKeyset& other);
  PublicKeyset& operator=(const PublicKeyset& other);
  PublicKeyset(PublicKeyset&& other);
  PublicKeyset& operator=(PublicKeyset&& other);
  ~PublicKeyset();

  std::vector<PublicKey> keys;
  base::Time fetch_time;
  // A null `expiry_time` indicates a response that should not be cached.
  base::Time expiry_time;

  static constexpr size_t kMaxNumberKeys = 10;
};

// Only used for logging.
CONTENT_EXPORT std::ostream& operator<<(std::ostream& out,
                                        const PublicKey& public_key);

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_PUBLIC_KEY_H_
