// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// crypto::keypair tests
#include "crypto/keypair.h"

#include "crypto/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using crypto::keypair::PrivateKey;
using crypto::keypair::PublicKey;

using crypto::test::FixedRsa2048PrivateKeyForTesting;
using crypto::test::FixedRsa4096PrivateKeyForTesting;

using crypto::test::FixedRsa2048PublicKeyForTesting;
using crypto::test::FixedRsa4096PublicKeyForTesting;

// Generate keys, roundtrip them through encoding/decoding to PrivateKeyInfo,
// and ensure the resulting key is equivalent. That gives some confidence that
// the generated key is actually a valid key because of the validation in
// FromPrivateKeyInfo().
TEST(Keypair, GenerateAndRoundtripPrivateKey) {
  auto expect_roundtrip = [](const PrivateKey& key) {
    auto k = PrivateKey::FromPrivateKeyInfo(key.ToPrivateKeyInfo());
    ASSERT_TRUE(k);
    EXPECT_EQ(key.ToPrivateKeyInfo(), k->ToPrivateKeyInfo());
    EXPECT_EQ(key.IsRsa(), k->IsRsa());
    EXPECT_EQ(key.IsEc(), k->IsEc());
  };

  expect_roundtrip(PrivateKey::GenerateRsa2048());
  expect_roundtrip(PrivateKey::GenerateRsa4096());
  expect_roundtrip(PrivateKey::GenerateEcP256());
}

// Export a public key from each private key and ensure it matches the expected
// public key.
TEST(Keypair, ExportPublicKey) {
  auto expect_export = [](const PrivateKey& priv, const PublicKey& pub) {
    EXPECT_EQ(priv.ToSubjectPublicKeyInfo(), pub.ToSubjectPublicKeyInfo());
  };

  expect_export(FixedRsa2048PrivateKeyForTesting(),
                FixedRsa2048PublicKeyForTesting());
  expect_export(FixedRsa4096PrivateKeyForTesting(),
                FixedRsa4096PublicKeyForTesting());
}

TEST(Keypair, ImportWithTrailingJunkFails) {
  auto priv = FixedRsa2048PrivateKeyForTesting().ToPrivateKeyInfo();
  auto pub = FixedRsa2048PublicKeyForTesting().ToSubjectPublicKeyInfo();

  EXPECT_TRUE(PrivateKey::FromPrivateKeyInfo(priv));
  priv.push_back(0);
  EXPECT_FALSE(PrivateKey::FromPrivateKeyInfo(priv));

  EXPECT_TRUE(PublicKey::FromSubjectPublicKeyInfo(pub));
  pub.push_back(0);
  EXPECT_FALSE(PublicKey::FromSubjectPublicKeyInfo(pub));
}

}  // namespace
