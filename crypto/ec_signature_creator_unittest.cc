// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/ec_signature_creator.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "crypto/ec_private_key.h"
#include "crypto/signature_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(rch): Add some exported keys from each to
// test interop between NSS and OpenSSL.

TEST(ECSignatureCreatorTest, BasicTest) {
  // Do a verify round trip.
  std::unique_ptr<crypto::ECPrivateKey> key_original(
      crypto::ECPrivateKey::Create());
  ASSERT_TRUE(key_original);

  std::vector<uint8_t> key_info;
  ASSERT_TRUE(key_original->ExportPrivateKey(&key_info));

  std::unique_ptr<crypto::ECPrivateKey> key(
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(key_info));
  ASSERT_TRUE(key);
  ASSERT_TRUE(key->key());

  std::unique_ptr<crypto::ECSignatureCreator> signer(
      crypto::ECSignatureCreator::Create(key.get()));
  ASSERT_TRUE(signer);

  std::string data("Hello, World!");
  std::vector<uint8_t> signature;
  ASSERT_TRUE(signer->Sign(base::as_bytes(base::make_span(data)), &signature));

  std::vector<uint8_t> public_key_info;
  ASSERT_TRUE(key_original->ExportPublicKey(&public_key_info));

  crypto::SignatureVerifier verifier;
  ASSERT_TRUE(verifier.VerifyInit(crypto::SignatureVerifier::ECDSA_SHA256,
                                  signature, public_key_info));

  verifier.VerifyUpdate(base::as_bytes(base::make_span(data)));
  ASSERT_TRUE(verifier.VerifyFinal());
}
