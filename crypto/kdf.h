// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_KDF_H_
#define CRYPTO_KDF_H_

#include "base/containers/span.h"
#include "crypto/crypto_export.h"
#include "crypto/hash.h"
#include "crypto/subtle_passkey.h"

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
// TODO(https://issues.chromium.org/issues/430635195): rename this.
CRYPTO_EXPORT void DeriveKeyPbkdf2HmacSha1(const Pbkdf2HmacSha1Params& params,
                                           base::span<const uint8_t> password,
                                           base::span<const uint8_t> salt,
                                           base::span<uint8_t> result,
                                           crypto::SubtlePassKey);

// TODO(https://issues.chromium.org/issues/369653192): document constraints on
// params.
// TODO(https://issues.chromium.org/issues/430635195): rename this.
//
// Note: this function CHECKs that the passed-in ScryptParams are valid. If you
// are not sure if your params will be valid, consult a //crypto OWNER - the
// definition of valid is somewhat tricky.
CRYPTO_EXPORT void DeriveKeyScrypt(const ScryptParams& params,
                                   base::span<const uint8_t> password,
                                   base::span<const uint8_t> salt,
                                   base::span<uint8_t> result,
                                   crypto::SubtlePassKey);

// Derive a key using HKDF with the specified hash kind, into the given out
// buffer, which must be the right size for that hash kind. The secret, salt,
// and info parameters have meanings as described in RFC 5869.
//
// Note that it's illegal to request more than 255 * the size of the output of
// the specified hash function. If you need large amounts of data generated from
// one key, you are better off using a keyed CSPRNG.
//
// TODO(https://issues.chromium.org/issues/431672006): recommend a specific
// keyed CSPRNG.
CRYPTO_EXPORT void Hkdf(crypto::hash::HashKind kind,
                        base::span<const uint8_t> secret,
                        base::span<const uint8_t> salt,
                        base::span<const uint8_t> info,
                        base::span<uint8_t> out);

// Same, but return an array containing the derived value. Templated on the
// array size.
template <size_t N>
std::array<uint8_t, N> Hkdf(crypto::hash::HashKind kind,
                            base::span<const uint8_t> secret,
                            base::span<const uint8_t> salt,
                            base::span<const uint8_t> info) {
  std::array<uint8_t, N> out;
  Hkdf(kind, secret, salt, info, out);
  return out;
}

}  // namespace crypto::kdf

#endif  // CRYPTO_KDF_H_
