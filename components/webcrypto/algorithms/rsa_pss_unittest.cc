// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/jwk.h"
#include "components/webcrypto/status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"

namespace webcrypto {

namespace {

blink::WebCryptoAlgorithm CreateRsaPssAlgorithm(
    unsigned int salt_length_bytes) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      blink::kWebCryptoAlgorithmIdRsaPss,
      new blink::WebCryptoRsaPssParams(salt_length_bytes));
}

class WebCryptoRsaPssTest : public WebCryptoTestBase {};

// Test that no two RSA-PSS signatures are identical, when using a non-zero
// lengthed salt.
TEST_F(WebCryptoRsaPssTest, SignIsRandom) {
  // Import public/private key pair.
  blink::WebCryptoKey public_key = blink::WebCryptoKey::CreateNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::CreateNull();

  ImportRsaKeyPair(
      HexStringToBytes(kPublicKeySpkiDerHex),
      HexStringToBytes(kPrivateKeyPkcs8DerHex),
      CreateRsaHashedImportAlgorithm(blink::kWebCryptoAlgorithmIdRsaPss,
                                     blink::kWebCryptoAlgorithmIdSha1),
      true, blink::kWebCryptoKeyUsageVerify, blink::kWebCryptoKeyUsageSign,
      &public_key, &private_key);

  // Use a 20-byte length salt.
  blink::WebCryptoAlgorithm params = CreateRsaPssAlgorithm(20);

  // Some random message to sign.
  std::vector<uint8_t> message = HexStringToBytes(kPublicKeySpkiDerHex);

  // Sign twice.
  std::vector<uint8_t> signature1;
  std::vector<uint8_t> signature2;

  ASSERT_EQ(Status::Success(),
            Sign(params, private_key, CryptoData(message), &signature1));
  ASSERT_EQ(Status::Success(),
            Sign(params, private_key, CryptoData(message), &signature2));

  // The signatures will be different because of the salt.
  EXPECT_NE(CryptoData(signature1), CryptoData(signature2));

  // However both signatures should work when verifying.
  bool is_match = false;

  ASSERT_EQ(Status::Success(),
            Verify(params, public_key, CryptoData(signature1),
                   CryptoData(message), &is_match));
  EXPECT_TRUE(is_match);

  ASSERT_EQ(Status::Success(),
            Verify(params, public_key, CryptoData(signature2),
                   CryptoData(message), &is_match));
  EXPECT_TRUE(is_match);

  // Corrupt the signature and verification must fail.
  ASSERT_EQ(Status::Success(),
            Verify(params, public_key, CryptoData(Corrupted(signature2)),
                   CryptoData(message), &is_match));
  EXPECT_FALSE(is_match);
}

// Try signing and verifying when the salt length is 0. The signature in this
// case is not random.
TEST_F(WebCryptoRsaPssTest, SignVerifyNoSalt) {
  // Import public/private key pair.
  blink::WebCryptoKey public_key = blink::WebCryptoKey::CreateNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::CreateNull();

  ImportRsaKeyPair(
      HexStringToBytes(kPublicKeySpkiDerHex),
      HexStringToBytes(kPrivateKeyPkcs8DerHex),
      CreateRsaHashedImportAlgorithm(blink::kWebCryptoAlgorithmIdRsaPss,
                                     blink::kWebCryptoAlgorithmIdSha1),
      true, blink::kWebCryptoKeyUsageVerify, blink::kWebCryptoKeyUsageSign,
      &public_key, &private_key);

  // Zero-length salt.
  blink::WebCryptoAlgorithm params = CreateRsaPssAlgorithm(0);

  // Some random message to sign.
  std::vector<uint8_t> message = HexStringToBytes(kPublicKeySpkiDerHex);

  // Sign twice.
  std::vector<uint8_t> signature1;
  std::vector<uint8_t> signature2;

  ASSERT_EQ(Status::Success(),
            Sign(params, private_key, CryptoData(message), &signature1));
  ASSERT_EQ(Status::Success(),
            Sign(params, private_key, CryptoData(message), &signature2));

  // The signatures will be the same this time.
  EXPECT_EQ(CryptoData(signature1), CryptoData(signature2));

  // Make sure that verification works.
  bool is_match = false;
  ASSERT_EQ(Status::Success(),
            Verify(params, public_key, CryptoData(signature1),
                   CryptoData(message), &is_match));
  EXPECT_TRUE(is_match);

  // Corrupt the signature and verification must fail.
  ASSERT_EQ(Status::Success(),
            Verify(params, public_key, CryptoData(Corrupted(signature2)),
                   CryptoData(message), &is_match));
  EXPECT_FALSE(is_match);
}

TEST_F(WebCryptoRsaPssTest, SignEmptyMessage) {
  // Import public/private key pair.
  blink::WebCryptoKey public_key = blink::WebCryptoKey::CreateNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::CreateNull();

  ImportRsaKeyPair(
      HexStringToBytes(kPublicKeySpkiDerHex),
      HexStringToBytes(kPrivateKeyPkcs8DerHex),
      CreateRsaHashedImportAlgorithm(blink::kWebCryptoAlgorithmIdRsaPss,
                                     blink::kWebCryptoAlgorithmIdSha1),
      true, blink::kWebCryptoKeyUsageVerify, blink::kWebCryptoKeyUsageSign,
      &public_key, &private_key);

  blink::WebCryptoAlgorithm params = CreateRsaPssAlgorithm(20);
  std::vector<uint8_t> message;  // Empty message.
  std::vector<uint8_t> signature;

  ASSERT_EQ(Status::Success(),
            Sign(params, private_key, CryptoData(message), &signature));

  // Make sure that verification works.
  bool is_match = false;
  ASSERT_EQ(Status::Success(), Verify(params, public_key, CryptoData(signature),
                                      CryptoData(message), &is_match));
  EXPECT_TRUE(is_match);

  // Corrupt the signature and verification must fail.
  ASSERT_EQ(Status::Success(),
            Verify(params, public_key, CryptoData(Corrupted(signature)),
                   CryptoData(message), &is_match));
  EXPECT_FALSE(is_match);
}

// Iterate through known answers and test verification.
//   * Verify over original message should succeed
//   * Verify over corrupted message should fail
//   * Verification with corrupted signature should fail
TEST_F(WebCryptoRsaPssTest, VerifyKnownAnswer) {
  base::DictionaryValue test_data;
  ASSERT_TRUE(ReadJsonTestFileToDictionary("rsa_pss.json", &test_data));

  const base::DictionaryValue* keys_dict = nullptr;
  ASSERT_TRUE(test_data.GetDictionary("keys", &keys_dict));

  const base::ListValue* tests = nullptr;
  ASSERT_TRUE(test_data.GetList("tests", &tests));

  for (size_t test_index = 0; test_index < tests->GetList().size();
       ++test_index) {
    SCOPED_TRACE(test_index);

    const base::Value& test_value = tests->GetList()[test_index];
    ASSERT_TRUE(test_value.is_dict());
    const base::DictionaryValue* test =
        &base::Value::AsDictionaryValue(test_value);

    blink::WebCryptoAlgorithm hash = GetDigestAlgorithm(test, "hash");

    std::string key_name;
    ASSERT_TRUE(test->GetString("key", &key_name));

    // Import the public key.
    blink::WebCryptoKey public_key = blink::WebCryptoKey::CreateNull();
    std::vector<uint8_t> spki_bytes =
        GetBytesFromHexString(keys_dict, key_name);

    ASSERT_EQ(Status::Success(),
              ImportKey(blink::kWebCryptoKeyFormatSpki, CryptoData(spki_bytes),
                        CreateRsaHashedImportAlgorithm(
                            blink::kWebCryptoAlgorithmIdRsaPss, hash.Id()),
                        true, blink::kWebCryptoKeyUsageVerify, &public_key));

    absl::optional<int> saltLength = test->FindIntKey("saltLength");
    ASSERT_TRUE(saltLength);

    std::vector<uint8_t> message = GetBytesFromHexString(test, "message");
    std::vector<uint8_t> signature = GetBytesFromHexString(test, "signature");

    // Test that verification returns true when it should.
    bool is_match = false;
    ASSERT_EQ(Status::Success(),
              Verify(CreateRsaPssAlgorithm(*saltLength), public_key,
                     CryptoData(signature), CryptoData(message), &is_match));
    EXPECT_TRUE(is_match);

    // Corrupt the message and make sure that verification fails.
    ASSERT_EQ(Status::Success(),
              Verify(CreateRsaPssAlgorithm(*saltLength), public_key,
                     CryptoData(signature), CryptoData(Corrupted(message)),
                     &is_match));
    EXPECT_FALSE(is_match);

    // Corrupt the signature and make sure that verification fails.
    ASSERT_EQ(Status::Success(),
              Verify(CreateRsaPssAlgorithm(*saltLength), public_key,
                     CryptoData(Corrupted(signature)), CryptoData(message),
                     &is_match));
    EXPECT_FALSE(is_match);
  }
}

}  // namespace

}  // namespace webcrypto
