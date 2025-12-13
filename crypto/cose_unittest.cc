// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/cose.h"

#include "base/test/gtest_util.h"
#include "crypto/keypair.h"
#include "crypto/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crypto {

using keypair::PrivateKey;
using keypair::PublicKey;
using test::FixedEcP256PublicKeyAsCoseForTesting;
using test::FixedEcP256PublicKeyForTesting;
using test::FixedRsa2048PublicKeyAsCoseForTesting;
using test::FixedRsa2048PublicKeyForTesting;

TEST(CoseTest, Rsa) {
  EXPECT_EQ(PublicKeyToCoseKey(FixedRsa2048PublicKeyForTesting()),
            FixedRsa2048PublicKeyAsCoseForTesting());
}

TEST(CoseTest, EcP256) {
  EXPECT_EQ(PublicKeyToCoseKey(FixedEcP256PublicKeyForTesting()),
            FixedEcP256PublicKeyAsCoseForTesting());
}

TEST(CoseTest, EdP256) {
  EXPECT_NOTREACHED_DEATH(PublicKeyToCoseKey(
      PublicKey::FromPrivateKey(PrivateKey::GenerateEd25519())));
}

}  // namespace crypto
