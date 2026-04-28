// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HPKE is a very flexible construct, with multiple modes and several different
// parameters. We only support modes and parameters that are used in Chromium;
// if you want support for a new mode or parameter, contact a //CRYPTO_OWNERS
// member.
//
// See RFC 9180 for the specification in its full glory.

#ifndef CRYPTO_HPKE_H_
#define CRYPTO_HPKE_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "crypto/crypto_export.h"
#include "crypto/keypair.h"

namespace crypto::hpke {

enum class KemType { kX25519HkdfSha256 };
enum class KdfType { kHkdfSha256 };
enum class AeadType { kChaCha20Poly1305, kAes128Gcm };

struct HpkeParams {
  KemType kem;
  KdfType kdf;
  AeadType aead;
};

// One-shot encryption in Auth mode.
// Returns a vector containing the encapsulated shared secret followed by the
// ciphertext, or nullopt on failure. See RFC 9180 section 6.1 for a
// description of this one-shot mode.
CRYPTO_EXPORT std::optional<std::vector<uint8_t>> AuthSeal(
    const HpkeParams& params,
    const crypto::keypair::PrivateKey& sender,
    const crypto::keypair::PublicKey& reciever,
    base::span<const uint8_t> plaintext,
    base::span<const uint8_t> info,
    base::span<const uint8_t> ad);

// One-shot decryption in Auth mode.
// Returns the decrypted plaintext, or nullopt on failure.
//
// `encrypted_data` should contain the encapsulated shared secret (32 bytes)
// followed by the ciphertext. See RFC 9180 section 6.1.
CRYPTO_EXPORT std::optional<std::vector<uint8_t>> AuthOpen(
    const HpkeParams& params,
    const crypto::keypair::PrivateKey& receiver,
    const crypto::keypair::PublicKey& sender,
    base::span<const uint8_t> encrypted_data,
    base::span<const uint8_t> info,
    base::span<const uint8_t> ad);

// One-shot encryption in Base mode.
// Returns a vector containing the encapsulated shared secret followed by the
// ciphertext, or nullopt on failure.
CRYPTO_EXPORT std::optional<std::vector<uint8_t>> Seal(
    const HpkeParams& params,
    const crypto::keypair::PublicKey& recipient,
    base::span<const uint8_t> plaintext,
    base::span<const uint8_t> info,
    base::span<const uint8_t> ad);

// One-shot decryption in Base mode.
// Returns the decrypted plaintext, or nullopt on failure.
//
// `encrypted_data` should contain the encapsulated shared secret (32 bytes)
// followed by the ciphertext.
CRYPTO_EXPORT std::optional<std::vector<uint8_t>> Open(
    const HpkeParams& params,
    const crypto::keypair::PrivateKey& receiver,
    base::span<const uint8_t> encrypted_data,
    base::span<const uint8_t> info,
    base::span<const uint8_t> ad);

}  // namespace crypto::hpke

#endif  // CRYPTO_HPKE_H_
