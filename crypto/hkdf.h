// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_HKDF_H_
#define CRYPTO_HKDF_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/containers/span.h"
#include "crypto/crypto_export.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"

namespace crypto {

CRYPTO_EXPORT
std::string HkdfSha256(std::string_view secret,
                       std::string_view salt,
                       std::string_view info,
                       size_t derived_key_size);

template <size_t KeySize>
std::array<uint8_t, KeySize> HkdfSha256(base::span<const uint8_t> secret,
                                        base::span<const uint8_t> salt,
                                        base::span<const uint8_t> info) {
  std::array<uint8_t, KeySize> ret;
  int result =
      ::HKDF(ret.data(), KeySize, EVP_sha256(), secret.data(), secret.size(),
             salt.data(), salt.size(), info.data(), info.size());
  DCHECK(result);
  return ret;
}

}  // namespace crypto

#endif  // CRYPTO_HKDF_H_
