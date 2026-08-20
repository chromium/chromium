// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_TEST_SUPPORT_H_
#define CRYPTO_TEST_SUPPORT_H_

#include "crypto/keypair.h"

namespace crypto::test {

using crypto::keypair::PrivateKey;
using crypto::keypair::PublicKey;

// These create and return PrivateKey instances that wrap fixed, pre-generated
// private keys for use in tests. Tests should prefer these keys over freshly
// generating keys whenever practical, since they are much cheaper.
PrivateKey FixedRsa2048PrivateKeyForTesting();
PublicKey FixedRsa2048PublicKeyForTesting();
const base::span<const uint8_t> FixedRsa2048PublicKeyAsCoseForTesting();

PrivateKey FixedRsa4096PrivateKeyForTesting();
PublicKey FixedRsa4096PublicKeyForTesting();

PrivateKey FixedEcP256PrivateKeyForTesting();
PublicKey FixedEcP256PublicKeyForTesting();
const base::span<const uint8_t> FixedEcP256PublicKeyAsCoseForTesting();

PrivateKey FixedMldsa44PrivateKeyForTesting();
PublicKey FixedMldsa44PublicKeyForTesting();
const base::span<const uint8_t> FixedMldsa44PublicKeyAsCoseForTesting();

PrivateKey FixedMldsa65PrivateKeyForTesting();
PublicKey FixedMldsa65PublicKeyForTesting();
const base::span<const uint8_t> FixedMldsa65PublicKeyAsCoseForTesting();

PrivateKey FixedMldsa87PrivateKeyForTesting();
PublicKey FixedMldsa87PublicKeyForTesting();
const base::span<const uint8_t> FixedMldsa87PublicKeyAsCoseForTesting();

PrivateKey FixedEd25519PrivateKeyForTesting();
PublicKey FixedEd25519PublicKeyForTesting();
const base::span<const uint8_t> FixedEd25519PublicKeyAsCoseForTesting();

}  // namespace crypto::test

#endif  // CRYPTO_TEST_SUPPORT_H_
