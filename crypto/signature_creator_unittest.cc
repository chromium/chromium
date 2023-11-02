// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/signature_creator.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/hash/sha1.h"
#include "crypto/rsa_private_key.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SignatureCreatorTest, BasicTest) {
  // Do a verify round trip.
  std::unique_ptr<crypto::RSAPrivateKey> key_original(
      crypto::RSAPrivateKey::Create(1024));
  ASSERT_TRUE(key_original.get());

  std::vector<uint8_t> key_info;
  key_original->ExportPrivateKey(&key_info);
  std::unique_ptr<crypto::RSAPrivateKey> key(
      crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(key_info));
  ASSERT_TRUE(key.get());

  std::unique_ptr<crypto::SignatureCreator> signer(
      crypto::SignatureCreator::Create(key.get(),
                                       crypto::SignatureCreator::SHA1));
  ASSERT_TRUE(signer.get());

  std::string data("Hello, World!");
  ASSERT_TRUE(signer->Update(reinterpret_cast<const uint8_t*>(data.c_str()),
                             data.size()));

  std::vector<uint8_t> signature;
  ASSERT_TRUE(signer->Final(&signature));

  std::vector<uint8_t> public_key_info;
  ASSERT_TRUE(key_original->ExportPublicKey(&public_key_info));

  crypto::SignatureVerifier verifier;
  ASSERT_TRUE(verifier.VerifyInit(crypto::SignatureVerifier::RSA_PKCS1_SHA1,
                                  signature, public_key_info));

  verifier.VerifyUpdate(base::as_bytes(base::make_span(data)));
  ASSERT_TRUE(verifier.VerifyFinal());
}

TEST(SignatureCreatorTest, SignDigestTest) {
  // Do a verify round trip.
  std::unique_ptr<crypto::RSAPrivateKey> key_original(
      crypto::RSAPrivateKey::Create(1024));
  ASSERT_TRUE(key_original.get());

  std::vector<uint8_t> key_info;
  key_original->ExportPrivateKey(&key_info);
  std::unique_ptr<crypto::RSAPrivateKey> key(
      crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(key_info));
  ASSERT_TRUE(key.get());

  std::string data("Hello, World!");
  std::string sha1 = base::SHA1HashString(data);
  // Sign sha1 of the input data.
  std::vector<uint8_t> signature;
  ASSERT_TRUE(crypto::SignatureCreator::Sign(
      key.get(), crypto::SignatureCreator::SHA1,
      reinterpret_cast<const uint8_t*>(sha1.c_str()), sha1.size(), &signature));

  std::vector<uint8_t> public_key_info;
  ASSERT_TRUE(key_original->ExportPublicKey(&public_key_info));

  // Verify the input data.
  crypto::SignatureVerifier verifier;
  ASSERT_TRUE(verifier.VerifyInit(crypto::SignatureVerifier::RSA_PKCS1_SHA1,
                                  signature, public_key_info));

  verifier.VerifyUpdate(base::as_bytes(base::make_span(data)));
  ASSERT_TRUE(verifier.VerifyFinal());
}

TEST(SignatureCreatorTest, SignSHA256DigestTest) {
  // Do a verify round trip.
  std::unique_ptr<crypto::RSAPrivateKey> key_original(
      crypto::RSAPrivateKey::Create(1024));
  ASSERT_TRUE(key_original.get());

  std::vector<uint8_t> key_info;
  key_original->ExportPrivateKey(&key_info);
  std::unique_ptr<crypto::RSAPrivateKey> key(
      crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(key_info));
  ASSERT_TRUE(key.get());

  std::string data("Hello, World!");
  std::string sha256 = crypto::SHA256HashString(data);
  // Sign sha256 of the input data.
  std::vector<uint8_t> signature;
  ASSERT_TRUE(crypto::SignatureCreator::Sign(
      key.get(), crypto::SignatureCreator::HashAlgorithm::SHA256,
      reinterpret_cast<const uint8_t*>(sha256.c_str()), sha256.size(),
      &signature));

  std::vector<uint8_t> public_key_info;
  ASSERT_TRUE(key_original->ExportPublicKey(&public_key_info));

  // Verify the input data.
  crypto::SignatureVerifier verifier;
  ASSERT_TRUE(verifier.VerifyInit(crypto::SignatureVerifier::RSA_PKCS1_SHA256,
                                  signature, public_key_info));

  verifier.VerifyUpdate(base::as_bytes(base::make_span(data)));
  ASSERT_TRUE(verifier.VerifyFinal());
}
