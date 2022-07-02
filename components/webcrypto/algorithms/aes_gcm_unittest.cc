// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/values.h"
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

Status AesGcmEncrypt(const blink::WebCryptoKey& key,
                     const std::vector<uint8_t>& iv,
                     const std::vector<uint8_t>& additional_data,
                     unsigned int tag_length_bits,
                     const std::vector<uint8_t>& plain_text,
                     std::vector<uint8_t>* cipher_text,
                     std::vector<uint8_t>* authentication_tag) {
  blink::WebCryptoAlgorithm algorithm =
      CreateAesGcmAlgorithm(iv, additional_data, tag_length_bits);

  std::vector<uint8_t> output;
  Status status = Encrypt(algorithm, key, plain_text, &output);
  if (status.IsError())
    return status;

  if ((tag_length_bits % 8) != 0) {
    ADD_FAILURE() << "Encrypt should have failed.";
    return Status::OperationError();
  }

  size_t tag_length_bytes = tag_length_bits / 8;

  if (tag_length_bytes > output.size()) {
    ADD_FAILURE() << "tag length is larger than output";
    return Status::OperationError();
  }

  // The encryption result is cipher text with authentication tag appended.
  cipher_text->assign(output.begin(),
                      output.begin() + (output.size() - tag_length_bytes));
  authentication_tag->assign(output.begin() + cipher_text->size(),
                             output.end());

  return Status::Success();
}

Status AesGcmDecrypt(const blink::WebCryptoKey& key,
                     const std::vector<uint8_t>& iv,
                     const std::vector<uint8_t>& additional_data,
                     unsigned int tag_length_bits,
                     const std::vector<uint8_t>& cipher_text,
                     const std::vector<uint8_t>& authentication_tag,
                     std::vector<uint8_t>* plain_text) {
  blink::WebCryptoAlgorithm algorithm =
      CreateAesGcmAlgorithm(iv, additional_data, tag_length_bits);

  // Join cipher text and authentication tag.
  std::vector<uint8_t> cipher_text_with_tag;
  cipher_text_with_tag.reserve(cipher_text.size() + authentication_tag.size());
  cipher_text_with_tag.insert(cipher_text_with_tag.end(), cipher_text.begin(),
                              cipher_text.end());
  cipher_text_with_tag.insert(cipher_text_with_tag.end(),
                              authentication_tag.begin(),
                              authentication_tag.end());

  return Decrypt(algorithm, key, cipher_text_with_tag, plain_text);
}

class WebCryptoAesGcmTest : public WebCryptoTestBase {};

TEST_F(WebCryptoAesGcmTest, GenerateKeyBadLength) {
  const uint16_t kKeyLen[] = {0, 127, 257};
  blink::WebCryptoKey key;
  for (size_t i = 0; i < std::size(kKeyLen); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(Status::ErrorGenerateAesKeyLength(),
              GenerateSecretKey(CreateAesGcmKeyGenAlgorithm(kKeyLen[i]), true,
                                blink::kWebCryptoKeyUsageDecrypt, &key));
  }
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

// TODO(eroman):
//   * Test decryption when the tag length exceeds input size
//   * Test decryption with empty input
//   * Test decryption with tag length of 0.
TEST_F(WebCryptoAesGcmTest, SampleSets) {
  base::Value::List tests = ReadJsonTestFileAsList("aes_gcm.json");

  // Note that WebCrypto appends the authentication tag to the ciphertext.
  for (const auto& test_value : tests) {
    SCOPED_TRACE(&test_value - &tests[0]);
    ASSERT_TRUE(test_value.is_dict());
    const base::DictionaryValue* test =
        &base::Value::AsDictionaryValue(test_value);

    const std::vector<uint8_t> test_key = GetBytesFromHexString(test, "key");
    const std::vector<uint8_t> test_iv = GetBytesFromHexString(test, "iv");
    const std::vector<uint8_t> test_additional_data =
        GetBytesFromHexString(test, "additional_data");
    const std::vector<uint8_t> test_plain_text =
        GetBytesFromHexString(test, "plain_text");
    const std::vector<uint8_t> test_authentication_tag =
        GetBytesFromHexString(test, "authentication_tag");
    const unsigned int test_tag_size_bits =
        static_cast<unsigned int>(test_authentication_tag.size()) * 8;
    const std::vector<uint8_t> test_cipher_text =
        GetBytesFromHexString(test, "cipher_text");

    blink::WebCryptoKey key = ImportSecretKeyFromRaw(
        test_key, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesGcm),
        blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt);

    // Verify exported raw key is identical to the imported data
    std::vector<uint8_t> raw_key;
    EXPECT_EQ(Status::Success(),
              ExportKey(blink::kWebCryptoKeyFormatRaw, key, &raw_key));

    EXPECT_BYTES_EQ(test_key, raw_key);

    // Test encryption.
    std::vector<uint8_t> cipher_text;
    std::vector<uint8_t> authentication_tag;
    EXPECT_EQ(
        Status::Success(),
        AesGcmEncrypt(key, test_iv, test_additional_data, test_tag_size_bits,
                      test_plain_text, &cipher_text, &authentication_tag));

    EXPECT_BYTES_EQ(test_cipher_text, cipher_text);
    EXPECT_BYTES_EQ(test_authentication_tag, authentication_tag);

    // Test decryption.
    std::vector<uint8_t> plain_text;
    EXPECT_EQ(
        Status::Success(),
        AesGcmDecrypt(key, test_iv, test_additional_data, test_tag_size_bits,
                      test_cipher_text, test_authentication_tag, &plain_text));
    EXPECT_BYTES_EQ(test_plain_text, plain_text);

    // Decryption should fail if any of the inputs are tampered with.
    EXPECT_EQ(Status::OperationError(),
              AesGcmDecrypt(key, Corrupted(test_iv), test_additional_data,
                            test_tag_size_bits, test_cipher_text,
                            test_authentication_tag, &plain_text));
    EXPECT_EQ(Status::OperationError(),
              AesGcmDecrypt(key, test_iv, Corrupted(test_additional_data),
                            test_tag_size_bits, test_cipher_text,
                            test_authentication_tag, &plain_text));
    EXPECT_EQ(Status::OperationError(),
              AesGcmDecrypt(key, test_iv, test_additional_data,
                            test_tag_size_bits, Corrupted(test_cipher_text),
                            test_authentication_tag, &plain_text));
    EXPECT_EQ(Status::OperationError(),
              AesGcmDecrypt(key, test_iv, test_additional_data,
                            test_tag_size_bits, test_cipher_text,
                            Corrupted(test_authentication_tag), &plain_text));

    // Try different incorrect tag lengths
    uint8_t kAlternateTagLengths[] = {0, 8, 96, 120, 128, 160, 255};
    for (size_t tag_i = 0; tag_i < std::size(kAlternateTagLengths); ++tag_i) {
      unsigned int wrong_tag_size_bits = kAlternateTagLengths[tag_i];
      if (test_tag_size_bits == wrong_tag_size_bits)
        continue;
      EXPECT_NE(Status::Success(),
                AesGcmDecrypt(key, test_iv, test_additional_data,
                              wrong_tag_size_bits, test_cipher_text,
                              test_authentication_tag, &plain_text));
    }
  }
}

}  // namespace

}  // namespace webcrypto
