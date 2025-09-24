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

}  // namespace crypto::test

#endif  // CRYPTO_TEST_SUPPORT_H_
