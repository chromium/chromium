// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_KEX_H_
#define CRYPTO_KEX_H_

#include "base/containers/span.h"
#include "crypto/keypair.h"

namespace crypto::kex {

// Derives a shared secret using elliptic-curve Diffie Hellman from a public key
// and a private key, and stores it in the provided out span. The resulting
// secret is not processed in any way and is not appropriate to use directly as
// key material (not all of the bits are uniformly random).
//
// The caller is responsible for ensuring that `theirs` and `ours` are P-256
// keys, e.g. with `IsEcP256`. Passing keys of the wrong type will cause the
// function to abort. Note that these conditions imply ECDH is infallible;
// `PublicKey` constructors enforce that P-256 keys are on the curve and not the
// point at infinity.
CRYPTO_EXPORT void EcdhP256(const crypto::keypair::PublicKey& theirs,
                            const crypto::keypair::PrivateKey& ours,
                            base::span<uint8_t, 32> out);

}  // namespace crypto::kex

#endif  // CRYPTO_KEX_H_
