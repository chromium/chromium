// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/rsa_key_pair.h"

#include <optional>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "crypto/signature_verifier.h"
#include "remoting/base/test_rsa_key_pair.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

// Simulates the client-side logic for signature verification.
bool VerifySignature(const std::string& host_public_key_base64,
                     const std::string& data,
                     const std::vector<uint8_t>& signature) {
  std::optional<std::vector<uint8_t>> host_public_key =
      base::Base64Decode(host_public_key_base64);
  if (!host_public_key.has_value()) {
    LOG(ERROR) << "Failed to decode public key: " << host_public_key_base64;
    return false;
  }
  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(crypto::SignatureVerifier::RSA_PSS_SHA256, signature,
                           *host_public_key)) {
    LOG(ERROR) << "Failed to initialize SignatureVerifier";
    return false;
  }
  verifier.VerifyUpdate(base::as_bytes(base::span(data)));
  return verifier.VerifyFinal();
}

}  // namespace

using RsaKeyPairTest = ::testing::Test;

TEST_F(RsaKeyPairTest, GenerateKey) {
  // Test that we can generate a valid key.
  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::Generate();
  ASSERT_TRUE(key_pair.get());
  ASSERT_NE(key_pair->ToString(), "");
  ASSERT_NE(key_pair->GetPublicKey(), "");
}

TEST_F(RsaKeyPairTest, FromString) {
  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::Generate();
  ASSERT_TRUE(key_pair.get());

  std::string key_string = key_pair->ToString();
  scoped_refptr<RsaKeyPair> key_pair2 = RsaKeyPair::FromString(key_string);
  ASSERT_TRUE(key_pair2.get());

  ASSERT_EQ(key_string, key_pair2->ToString());
  ASSERT_EQ(key_pair->GetPublicKey(), key_pair2->GetPublicKey());

  // Test that signing with the new key works.
  std::string data = "data";
  std::vector<uint8_t> signature =
      key_pair2->Sign(base::as_bytes(base::span(data)));
  ASSERT_TRUE(VerifySignature(key_pair->GetPublicKey(), data, signature));
}

TEST_F(RsaKeyPairTest, GenerateCertificate) {
  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::Generate();
  ASSERT_TRUE(key_pair.get());

  std::string cert = key_pair->GenerateCertificate();
  ASSERT_FALSE(cert.empty());
}

TEST_F(RsaKeyPairTest, Sign) {
  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::Generate();
  ASSERT_TRUE(key_pair.get());

  std::string data = "data to be signed";
  std::vector<uint8_t> signature =
      key_pair->Sign(base::as_bytes(base::span(data)));

  ASSERT_FALSE(signature.empty());

  ASSERT_TRUE(VerifySignature(key_pair->GetPublicKey(), data, signature));

  // Test that verification fails for different data.
  ASSERT_FALSE(
      VerifySignature(key_pair->GetPublicKey(), "invalid data", signature));

  // Test that verification fails for a different key.
  scoped_refptr<RsaKeyPair> key_pair2 = RsaKeyPair::Generate();
  ASSERT_TRUE(key_pair2.get());
  ASSERT_FALSE(VerifySignature(key_pair2->GetPublicKey(), data, signature));

  // Test that verification fails for a corrupted signature.
  std::vector<uint8_t> corrupted_signature = signature;
  corrupted_signature[0] ^= 0x10;
  ASSERT_FALSE(
      VerifySignature(key_pair->GetPublicKey(), data, corrupted_signature));
}

}  // namespace remoting
