// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_KDF_H_
#define CRYPTO_KDF_H_

#include "crypto/crypto_export.h"
#include "crypto/subtle_passkey.h"
#include "crypto/symmetric_key.h"

namespace crypto::kdf {

// A KDF (key derivation function) produces key material from a secret input, a
// salt, and a set of parameters controlling how much work the KDF should
// perform. They are used for:
// - Generating subkeys from a main key, or
// - Deriving keys from a cryptographically-weak secret like a password, in such
//   a way that it is more difficult to mount a brute-force attack
//
// The KDFs themselves are free functions that take parameter structs. You will
// need a crypto::SubtlePassKey to call these since choosing the parameters
// requires some caution.
//
// TODO(https://issues.chromium.org/issues/369653192): add a sensible-default
// KDF that doesn't require a passkey.

struct Pbkdf2HmacSha1Params {
  // BoringSSL uses a uint32_t for the iteration count for PBKDF2, so we match
  // that.
  uint32_t iterations;
};

struct ScryptParams {
  // These all match the relevant types in BoringSSL.
  uint64_t cost;                  // aka 'N' in RFC 7914
  uint64_t block_size;            // aka 'r' in RFC 7914
  uint64_t parallelization;       // aka 'p' in RFC 7914
  uint64_t max_memory_bytes = 0;  // doesn't appear in the RFC
};

// TODO(https://issues.chromium.org/issues/369653192): document constraints on
// params.
CRYPTO_EXPORT void DeriveKeyPbkdf2HmacSha1(const Pbkdf2HmacSha1Params& params,
                                           base::span<const uint8_t> password,
                                           base::span<const uint8_t> salt,
                                           base::span<uint8_t> result,
                                           crypto::SubtlePassKey);

// TODO(https://issues.chromium.org/issues/369653192): document constraints on
// params.
//
// Note: this function CHECKs that the passed-in ScryptParams are valid. If you
// are not sure if your params will be valid, consult a //crypto OWNER - the
// definition of valid is somewhat tricky.
CRYPTO_EXPORT void DeriveKeyScrypt(const ScryptParams& params,
                                   base::span<const uint8_t> password,
                                   base::span<const uint8_t> salt,
                                   base::span<uint8_t> result,
                                   crypto::SubtlePassKey);

}  // namespace crypto::kdf

#endif  // CRYPTO_KDF_H_
