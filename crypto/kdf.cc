// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/kdf.h"

#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace crypto::kdf {

void DeriveKeyPbkdf2HmacSha1(const Pbkdf2HmacSha1Params& params,
                             base::span<const uint8_t> password,
                             base::span<const uint8_t> salt,
                             base::span<uint8_t> result,
                             crypto::SubtlePassKey) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int rv = PKCS5_PBKDF2_HMAC_SHA1(
      base::as_chars(password).data(), password.size(), salt.data(),
      salt.size(), params.iterations, result.size(), result.data());

  CHECK_EQ(rv, 1);
}

void DeriveKeyScrypt(const ScryptParams& params,
                     base::span<const uint8_t> password,
                     base::span<const uint8_t> salt,
                     base::span<uint8_t> result,
                     crypto::SubtlePassKey) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int rv =
      EVP_PBE_scrypt(reinterpret_cast<const char*>(password.data()),
                     password.size(), salt.data(), salt.size(), params.cost,
                     params.block_size, params.parallelization,
                     params.max_memory_bytes, result.data(), result.size());

  CHECK_EQ(rv, 1);
}

}  // namespace crypto::kdf
