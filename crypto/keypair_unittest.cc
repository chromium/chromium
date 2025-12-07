// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// crypto::keypair tests
#include "crypto/keypair.h"

#include <list>

#include "base/containers/contains.h"
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
    EXPECT_EQ(key.IsEd25519(), k->IsEd25519());
  };

  expect_roundtrip(PrivateKey::GenerateRsa2048());
  expect_roundtrip(PrivateKey::GenerateRsa4096());
  expect_roundtrip(PrivateKey::GenerateEcP256());
  expect_roundtrip(PrivateKey::GenerateEcP384());
  expect_roundtrip(PrivateKey::GenerateEcP521());
  expect_roundtrip(PrivateKey::GenerateEd25519());
}

TEST(Keypair, RoundtripEd25519Key) {
  auto k = PrivateKey::GenerateEd25519();
  auto priv = k.ToEd25519PrivateKey();
  auto nk = PrivateKey::FromEd25519PrivateKey(priv);
  EXPECT_EQ(k.ToPrivateKeyInfo(), nk.ToPrivateKeyInfo());

  auto pub = k.ToEd25519PublicKey();
  auto npk = PublicKey::FromEd25519PublicKey(pub);
  EXPECT_EQ(k.ToSubjectPublicKeyInfo(), npk.ToSubjectPublicKeyInfo());
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

TEST(Keypair, PrivateKeyPredicates) {
  EXPECT_TRUE(FixedRsa2048PrivateKeyForTesting().IsRsa());
  auto p256 = PrivateKey::GenerateEcP256();
  EXPECT_TRUE(p256.IsEc() && p256.IsEcP256());
  auto p384 = PrivateKey::GenerateEcP384();
  EXPECT_TRUE(p384.IsEc() && p384.IsEcP384());
  auto p521 = PrivateKey::GenerateEcP521();
  EXPECT_TRUE(p521.IsEc() && p521.IsEcP521());
  EXPECT_TRUE(PrivateKey::GenerateEd25519().IsEd25519());
}

TEST(Keypair, PublicKeyPredicates) {
  EXPECT_TRUE(FixedRsa2048PublicKeyForTesting().IsRsa());
  auto p256 = PublicKey::FromPrivateKey(PrivateKey::GenerateEcP256());
  EXPECT_TRUE(p256.IsEc() && p256.IsEcP256());
  auto p384 = PublicKey::FromPrivateKey(PrivateKey::GenerateEcP384());
  EXPECT_TRUE(p384.IsEc() && p384.IsEcP384());
  auto p521 = PublicKey::FromPrivateKey(PrivateKey::GenerateEcP521());
  EXPECT_TRUE(p521.IsEc() && p521.IsEcP521());
  EXPECT_TRUE(
      PublicKey::FromPrivateKey(PrivateKey::GenerateEd25519()).IsEd25519());
}

TEST(Keypair, X962UncompressedForm) {
  auto expect_uncompressed_length = [](const PrivateKey& key, size_t len) {
    auto uncompressed = key.ToUncompressedX962Point();
    EXPECT_EQ(uncompressed.size(), len);
  };

  // It should be possible to convert EC keys on various curves into
  // uncompressed forms.
  expect_uncompressed_length(PrivateKey::GenerateEcP256(), 32 * 2 + 1);
  expect_uncompressed_length(PrivateKey::GenerateEcP384(), 48 * 2 + 1);
  // 521 bits = 66 bytes per coordinate
  expect_uncompressed_length(PrivateKey::GenerateEcP521(), 66 * 2 + 1);
}

TEST(Keypair, ImportUncompressed) {
  {
    auto p256_priv = PrivateKey::GenerateEcP256();
    auto p256_pub = PublicKey::FromPrivateKey(p256_priv);
    auto p256_import =
        PublicKey::FromEcP256Point(p256_priv.ToUncompressedX962Point());
    ASSERT_TRUE(p256_import);
    EXPECT_EQ(p256_pub.ToSubjectPublicKeyInfo(),
              p256_import->ToSubjectPublicKeyInfo());
  }

  {
    auto p384_priv = PrivateKey::GenerateEcP384();
    auto p384_pub = PublicKey::FromPrivateKey(p384_priv);
    auto p384_import =
        PublicKey::FromEcP384Point(p384_priv.ToUncompressedX962Point());
    ASSERT_TRUE(p384_import);
    EXPECT_EQ(p384_pub.ToSubjectPublicKeyInfo(),
              p384_import->ToSubjectPublicKeyInfo());
  }

  {
    auto p521_priv = PrivateKey::GenerateEcP521();
    auto p521_pub = PublicKey::FromPrivateKey(p521_priv);
    auto p521_import =
        PublicKey::FromEcP521Point(p521_priv.ToUncompressedX962Point());
    ASSERT_TRUE(p521_import);
    EXPECT_EQ(p521_pub.ToSubjectPublicKeyInfo(),
              p521_import->ToSubjectPublicKeyInfo());
  }
}

TEST(Keypair, RsaPublicComponents) {
  const auto key = FixedRsa2048PublicKeyForTesting();

  const auto n = key.GetRsaModulus();
  const auto e = key.GetRsaExponent();

  const auto new_key = PublicKey::FromRsaPublicKeyComponents(n, e);
  ASSERT_TRUE(new_key.has_value());
  EXPECT_EQ(key.ToSubjectPublicKeyInfo(), new_key->ToSubjectPublicKeyInfo());
}

}  // namespace
