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
using test::FixedEd25519PublicKeyAsCoseForTesting;
using test::FixedEd25519PublicKeyForTesting;
using test::FixedMldsa44PublicKeyAsCoseForTesting;
using test::FixedMldsa44PublicKeyForTesting;
using test::FixedMldsa65PublicKeyAsCoseForTesting;
using test::FixedMldsa65PublicKeyForTesting;
using test::FixedMldsa87PublicKeyAsCoseForTesting;
using test::FixedMldsa87PublicKeyForTesting;
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

TEST(CoseTest, Ed25519) {
  EXPECT_EQ(PublicKeyToCoseKey(FixedEd25519PublicKeyForTesting()),
            FixedEd25519PublicKeyAsCoseForTesting());
}

TEST(CoseTest, Mldsa44) {
  EXPECT_EQ(PublicKeyToCoseKey(FixedMldsa44PublicKeyForTesting()),
            FixedMldsa44PublicKeyAsCoseForTesting());
}

TEST(CoseTest, Mldsa65) {
  EXPECT_EQ(PublicKeyToCoseKey(FixedMldsa65PublicKeyForTesting()),
            FixedMldsa65PublicKeyAsCoseForTesting());
}

TEST(CoseTest, Mldsa87) {
  EXPECT_EQ(PublicKeyToCoseKey(FixedMldsa87PublicKeyForTesting()),
            FixedMldsa87PublicKeyAsCoseForTesting());
}

TEST(CoseTest, EcP384) {
  EXPECT_NOTREACHED_DEATH(PublicKeyToCoseKey(
      PublicKey::FromPrivateKey(PrivateKey::GenerateEcP384())));
}

}  // namespace crypto
