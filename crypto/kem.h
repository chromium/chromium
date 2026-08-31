// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This matches the RFC 9629 KEM interface, except that instead of using
// KeyGen(), you generate keys using the regular crypto/keypair interfaces.

#ifndef CRYPTO_KEM_H_
#define CRYPTO_KEM_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "crypto/crypto_export.h"
#include "crypto/keypair.h"

namespace crypto::kem {

enum Kem {
  kMlkem768,
};

// Decapsulate a ciphertext from a corresponding Encapsulate() call, returning
// the shared secret. This CHECKs that the given private key is appropriate for
// use with the given mechanism. May return nullopt if decapsulation fails; for
// certain KEMs, may return a non-nullopt *but still invalid* shared secret, so
// a non-nullopt return does not indicate that the ciphertext is intact.
CRYPTO_EXPORT std::optional<std::vector<uint8_t>> Decapsulate(
    Kem mech,
    const crypto::keypair::PrivateKey& privkey,
    base::span<const uint8_t> ciphertext);

struct CRYPTO_EXPORT EncapResult {
  std::vector<uint8_t> ciphertext;
  std::vector<uint8_t> secret;
};

// Encapsulate a new, random shared secret, returning the shared secret and
// a ciphertext to send to the recipient. This CHECKs that the given public
// key is appropriate for use with the given mechanism.
CRYPTO_EXPORT EncapResult Encapsulate(Kem mech,
                                      const crypto::keypair::PublicKey& pubkey);

}  // namespace crypto::kem

#endif  // CRYPTO_KEM_H_
