// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/test/gmock_expected_support.h"
#include "base/types/expected.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"

namespace webcrypto {

namespace {

// Creates an AES-GCM algorithm.
blink::WebCryptoAlgorithm CreateAesGcmAlgorithm(
    const std::vector<uint8_t>& iv,
    const std::vector<uint8_t>& additional_data,
    unsigned int tag_length_bits) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      blink::kWebCryptoAlgorithmIdAesGcm,
      new blink::WebCryptoAesGcmParams(iv, true, additional_data, true,
                                       tag_length_bits));
}

blink::WebCryptoAlgorithm CreateAesGcmKeyGenAlgorithm(
    uint16_t key_length_bits) {
  return CreateAesKeyGenAlgorithm(blink::kWebCryptoAlgorithmIdAesGcm,
                                  key_length_bits);
}

base::expected<std::vector<uint8_t>, Status> AesGcmEncrypt(
    const blink::WebCryptoKey& key,
    const std::vector<uint8_t>& iv,
    const std::vector<uint8_t>& additional_data,
    size_t tag_length_bits,
    const std::vector<uint8_t>& plain_text) {
  blink::WebCryptoAlgorithm algorithm =
      CreateAesGcmAlgorithm(iv, additional_data, tag_length_bits);

  std::vector<uint8_t> output;
  Status status = Encrypt(algorithm, key, plain_text, &output);
  if (status.IsError()) {
    return base::unexpected(status);
  }
  return output;
}

base::expected<std::vector<uint8_t>, Status> AesGcmDecrypt(
    const blink::WebCryptoKey& key,
    const std::vector<uint8_t>& iv,
    const std::vector<uint8_t>& additional_data,
    unsigned int tag_length_bits,
    const std::vector<uint8_t>& ciphertext) {
  blink::WebCryptoAlgorithm algorithm =
      CreateAesGcmAlgorithm(iv, additional_data, tag_length_bits);

  std::vector<uint8_t> output;
  Status status = Decrypt(algorithm, key, ciphertext, &output);
  if (status.IsError()) {
    return base::unexpected(status);
  }
  return output;
}

class WebCryptoAesGcmTest : public WebCryptoTestBase {};

TEST_F(WebCryptoAesGcmTest, GenerateKeyBadLength) {
  auto generate_key = [](size_t len) {
    blink::WebCryptoKey key;
    return GenerateSecretKey(CreateAesGcmKeyGenAlgorithm(len), true,
                             blink::kWebCryptoKeyUsageDecrypt, &key);
  };
  EXPECT_EQ(generate_key(0), Status::ErrorGenerateAesKeyLength());
  EXPECT_EQ(generate_key(127), Status::ErrorGenerateAesKeyLength());
  EXPECT_EQ(generate_key(257), Status::ErrorGenerateAesKeyLength());
}

TEST_F(WebCryptoAesGcmTest, GenerateKeyEmptyUsage) {
  blink::WebCryptoKey key;
  EXPECT_EQ(Status::ErrorCreateKeyEmptyUsages(),
            GenerateSecretKey(CreateAesGcmKeyGenAlgorithm(256), true, 0, &key));
}

TEST_F(WebCryptoAesGcmTest, ImportExportJwk) {
  const blink::WebCryptoAlgorithm algorithm =
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesGcm);

  // AES-GCM 128
  ImportExportJwkSymmetricKey(
      128, algorithm,
      blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt,
      "A128GCM");

  // AES-GCM 256
  ImportExportJwkSymmetricKey(256, algorithm, blink::kWebCryptoKeyUsageDecrypt,
                              "A256GCM");
}

struct AesGcmKnownAnswer {
  const char* key;
  const char* iv;
  const char* plaintext;
  const char* additional;
  const char* ciphertext;
  size_t tagbits;
};

// NIST GCM test vectors:
// http://csrc.nist.gov/groups/STM/cavp/documents/mac/gcmtestvectors.zip
const AesGcmKnownAnswer kAesGcmKnownAnswers[] = {
    {"cf063a34d4a9a76c2c86787d3f96db71", "113b9785971864c83b01c787", "", "",
     "72ac8493e3a5228b5d130a69d2510e42", 128},
    {"6dfa1a07c14f978020ace450ad663d18", "34edfa462a14c6969a680ec1", "",
     "2a35c7f5f8578e919a581c60500c04f6", "751f3098d59cf4ea1d2fb0853bde1c", 120},
    {"ed6cd876ceba555706674445c229c12d", "92ecbf74b765bc486383ca2e",
     "bfaaaea3880d72d4378561e2597a9b35", "95bd10d77dbe0e87fb34217f1a2e5efe",
     "bdd2ed6c66fa087dce617d7fd1ff6d93ba82e49c55a22ed02ca67da4ec6f", 112},
    {"e03548984a7ec8eaf0870637df0ac6bc17f7159315d0ae26a764fd224e483810",
     "f4feb26b846be4cd224dbc5133a5ae13814ebe19d3032acdd3a006463fdb71e83a9d5d966"
     "79f26cc1719dd6b4feb3bab5b4b7993d0c0681f36d105ad3002fb66b201538e2b7479838a"
     "b83402b0d816cd6e0fe5857e6f4adf92de8ee72b122ba1ac81795024943b7d0151bbf84ce"
     "87c8911f512c397d14112296da7ecdd0da52a",
     "69fd0c9da10b56ec6786333f8d76d4b74f8a434195f2f241f088b2520fb5fa29455df9893"
     "164fb1638abe6617915d9497a8fe2",
     "aab26eb3e7acd09a034a9e2651636ab3868e51281590ecc948355e457da42b7ad1391c7be"
     "0d9e82895e506173a81857c3226829fbd6dfb3f9657a71a2934445d7c05fa9401cddd5109"
     "016ba32c3856afaadc48de80b8a01b57cb",
     "fda718aa1ec163487e21afc34f5a3a34795a9ee71dd3e7ee9a18fdb24181dc982b29c6ec7"
     "23294a130ca2234952bb0ef68c0f34795fbe0",
     32},
};

// TODO(eroman):
//   * Test decryption when the tag length exceeds input size
//   * Test decryption with empty input
//   * Test decryption with tag length of 0.
TEST_F(WebCryptoAesGcmTest, KnownAnswers) {
  // Note that WebCrypto appends the authentication tag to the ciphertext.
  for (const auto& test : kAesGcmKnownAnswers) {
    SCOPED_TRACE(&test - &kAesGcmKnownAnswers[0]);

    auto key_bytes = HexStringToBytes(test.key);
    auto iv_bytes = HexStringToBytes(test.iv);
    auto plaintext_bytes = HexStringToBytes(test.plaintext);
    auto additional_bytes = HexStringToBytes(test.additional);
    auto ciphertext_bytes = HexStringToBytes(test.ciphertext);

    blink::WebCryptoKey key = ImportSecretKeyFromRaw(
        key_bytes, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesGcm),
        blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt);

    {
      std::vector<uint8_t> exported_key;
      EXPECT_EQ(Status::Success(),
                ExportKey(blink::kWebCryptoKeyFormatRaw, key, &exported_key));
      EXPECT_BYTES_EQ(key_bytes, exported_key);
    }

    ASSERT_OK_AND_ASSIGN(auto encrypt_result,
                         AesGcmEncrypt(key, iv_bytes, additional_bytes,
                                       test.tagbits, plaintext_bytes));
    EXPECT_BYTES_EQ(encrypt_result, ciphertext_bytes);

    ASSERT_OK_AND_ASSIGN(auto decrypt_result,
                         AesGcmDecrypt(key, iv_bytes, additional_bytes,
                                       test.tagbits, ciphertext_bytes));
    EXPECT_BYTES_EQ(decrypt_result, plaintext_bytes);

    // Decryption should fail if any of the inputs are tampered with.
    EXPECT_THAT(AesGcmDecrypt(key, Corrupted(iv_bytes), additional_bytes,
                              test.tagbits, ciphertext_bytes),
                base::test::ErrorIs(Status::OperationError()));
    EXPECT_THAT(AesGcmDecrypt(key, iv_bytes, Corrupted(additional_bytes),
                              test.tagbits, ciphertext_bytes),
                base::test::ErrorIs(Status::OperationError()));
    EXPECT_THAT(AesGcmDecrypt(key, iv_bytes, additional_bytes, test.tagbits,
                              Corrupted(ciphertext_bytes)),
                base::test::ErrorIs(Status::OperationError()));

    // Try different incorrect tag lengths
    for (unsigned int length : {0, 8, 96, 120, 128, 160, 255}) {
      if (test.tagbits == length)
        continue;
      EXPECT_FALSE(AesGcmDecrypt(key, iv_bytes, additional_bytes, length,
                                 ciphertext_bytes)
                       .has_value());
    }
  }
}

}  // namespace

}  // namespace webcrypto
