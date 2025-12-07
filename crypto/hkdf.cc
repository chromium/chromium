// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/hkdf.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/check.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"

namespace crypto {

std::string HkdfSha256(std::string_view secret,
                       std::string_view salt,
                       std::string_view info,
                       size_t derived_key_size) {
  std::string key;
  key.resize(derived_key_size);
  int result = ::HKDF(
      reinterpret_cast<uint8_t*>(&key[0]), derived_key_size, EVP_sha256(),
      reinterpret_cast<const uint8_t*>(secret.data()), secret.size(),
      reinterpret_cast<const uint8_t*>(salt.data()), salt.size(),
      reinterpret_cast<const uint8_t*>(info.data()), info.size());
  DCHECK(result);
  return key;
}

}  // namespace crypto
