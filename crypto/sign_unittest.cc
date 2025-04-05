// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/sign.h"

#include "crypto/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using crypto::keypair::PrivateKey;
using crypto::keypair::PublicKey;
using crypto::sign::SignatureKind;

TEST(Sign, RoundTripSignVerify) {
  constexpr auto data = std::to_array<uint8_t>({
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
      0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
      0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  });

  auto expect_roundtrip = [&](const PrivateKey& priv, const PublicKey& pub,
                              SignatureKind kind) {
    auto sig = crypto::sign::Sign(kind, priv, data);
    EXPECT_TRUE(crypto::sign::Verify(kind, pub, data, sig));
  };

  auto rsa_priv = crypto::test::FixedRsa2048PrivateKeyForTesting();
  auto rsa_pub = crypto::test::FixedRsa2048PublicKeyForTesting();

  expect_roundtrip(rsa_priv, rsa_pub, SignatureKind::RSA_PKCS1_SHA1);
  expect_roundtrip(rsa_priv, rsa_pub, SignatureKind::RSA_PKCS1_SHA256);
  expect_roundtrip(rsa_priv, rsa_pub, SignatureKind::RSA_PSS_SHA256);

  auto ec_priv = PrivateKey::GenerateEcP256();
  auto ec_pub = PublicKey::FromPrivateKey(ec_priv);

  expect_roundtrip(ec_priv, ec_pub, SignatureKind::ECDSA_SHA256);
}

}  // namespace
