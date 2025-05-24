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
PrivateKey FixedRsa4096PrivateKeyForTesting();

PublicKey FixedRsa2048PublicKeyForTesting();
PublicKey FixedRsa4096PublicKeyForTesting();

}  // namespace crypto::test

#endif  // CRYPTO_TEST_SUPPORT_H_
