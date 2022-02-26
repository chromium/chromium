// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/values.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/status.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"

namespace webcrypto {

namespace {

// Creates an AES-CTR algorithm for encryption/decryption.
blink::WebCryptoAlgorithm CreateAesCtrAlgorithm(
    const std::vector<uint8_t>& counter,
    uint8_t length_bits) {
  return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
      blink::kWebCryptoAlgorithmIdAesCtr,
      new blink::WebCryptoAesCtrParams(length_bits, counter));
}

class WebCryptoAesCtrTest : public WebCryptoTestBase {};

TEST_F(WebCryptoAesCtrTest, EncryptDecryptKnownAnswer) {
  base::Value tests;
  ASSERT_TRUE(ReadJsonTestFileAsList("aes_ctr.json", &tests));

  for (size_t test_index = 0; test_index < tests.GetListDeprecated().size();
       ++test_index) {
    SCOPED_TRACE(test_index);
    const base::Value& test_value = tests.GetListDeprecated()[test_index];
    ASSERT_TRUE(test_value.is_dict());
    const base::DictionaryValue* test =
        &base::Value::AsDictionaryValue(test_value);

    std::vector<uint8_t> test_key = GetBytesFromHexString(test, "key");
    std::vector<uint8_t> test_counter = GetBytesFromHexString(test, "counter");
    absl::optional<int> counter_length_bits = test->FindIntKey("length");
    ASSERT_TRUE(counter_length_bits);

    std::vector<uint8_t> test_plain_text =
        GetBytesFromHexString(test, "plain_text");
    std::vector<uint8_t> test_cipher_text =
        GetBytesFromHexString(test, "cipher_text");

    blink::WebCryptoKey key = ImportSecretKeyFromRaw(
        test_key, CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCtr),
        blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt);

    EXPECT_EQ(test_key.size() * 8, key.Algorithm().AesParams()->LengthBits());

    std::vector<uint8_t> output;

    // Test encryption.
    EXPECT_EQ(Status::Success(),
              Encrypt(CreateAesCtrAlgorithm(test_counter, *counter_length_bits),
                      key, CryptoData(test_plain_text), &output));
    EXPECT_BYTES_EQ(test_cipher_text, output);

    // Test decryption.
    EXPECT_EQ(Status::Success(),
              Decrypt(CreateAesCtrAlgorithm(test_counter, *counter_length_bits),
                      key, CryptoData(test_cipher_text), &output));
    EXPECT_BYTES_EQ(test_plain_text, output);
  }
}

// The counter block must be exactly 16 bytes.
TEST_F(WebCryptoAesCtrTest, InvalidCounterBlockLength) {
  const unsigned int kBadCounterBlockLengthBytes[] = {0, 15, 17};

  blink::WebCryptoKey key = ImportSecretKeyFromRaw(
      std::vector<uint8_t>(16),  // 128-bit key of all zeros.
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCtr),
      blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt);

  std::vector<uint8_t> input(32);
  std::vector<uint8_t> output;

  for (size_t i = 0; i < std::size(kBadCounterBlockLengthBytes); ++i) {
    std::vector<uint8_t> bad_counter(kBadCounterBlockLengthBytes[i]);

    EXPECT_EQ(Status::ErrorIncorrectSizeAesCtrCounter(),
              Encrypt(CreateAesCtrAlgorithm(bad_counter, 128), key,
                      CryptoData(input), &output));

    EXPECT_EQ(Status::ErrorIncorrectSizeAesCtrCounter(),
              Decrypt(CreateAesCtrAlgorithm(bad_counter, 128), key,
                      CryptoData(input), &output));
  }
}

// The counter length cannot be less than 1 or greater than 128.
TEST_F(WebCryptoAesCtrTest, InvalidCounterLength) {
  const uint8_t kBadCounterLengthBits[] = {0, 129};

  blink::WebCryptoKey key = ImportSecretKeyFromRaw(
      std::vector<uint8_t>(16),  // 128-bit key of all zeros.
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCtr),
      blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt);

  std::vector<uint8_t> counter(16);
  std::vector<uint8_t> input(32);
  std::vector<uint8_t> output;

  for (size_t i = 0; i < std::size(kBadCounterLengthBits); ++i) {
    uint8_t bad_counter_length_bits = kBadCounterLengthBits[i];

    EXPECT_EQ(Status::ErrorInvalidAesCtrCounterLength(),
              Encrypt(CreateAesCtrAlgorithm(counter, bad_counter_length_bits),
                      key, CryptoData(input), &output));

    EXPECT_EQ(Status::ErrorInvalidAesCtrCounterLength(),
              Decrypt(CreateAesCtrAlgorithm(counter, bad_counter_length_bits),
                      key, CryptoData(input), &output));
  }
}

// Tests wrap-around using a 4-bit counter.
//
// Wrap-around is allowed, however if the counter repeats itself an error should
// be thrown.
//
// Using a 4-bit counter it is possible to encrypt 16 blocks. However the 17th
// block would end up wrapping back to the starting value.
TEST_F(WebCryptoAesCtrTest, OverflowAndRepeatCounter) {
  const uint8_t kCounterLengthBits = 4;
  const uint8_t kStartCounter[] = {0, 1, 15};

  blink::WebCryptoKey key = ImportSecretKeyFromRaw(
      std::vector<uint8_t>(16),  // 128-bit key of all zeros.
      CreateAlgorithm(blink::kWebCryptoAlgorithmIdAesCtr),
      blink::kWebCryptoKeyUsageEncrypt | blink::kWebCryptoKeyUsageDecrypt);

  std::vector<uint8_t> buffer(272);

  // 16 and 17 AES blocks worth of data respectively (AES blocks are 16 bytes
  // long).
  CryptoData input_16(buffer.data(), 256);
  CryptoData input_17(buffer.data(), 272);

  std::vector<uint8_t> output;

  for (size_t i = 0; i < std::size(kStartCounter); ++i) {
    std::vector<uint8_t> counter(16);
    counter[15] = kStartCounter[i];

    // Baseline test: Encrypting 16 blocks should work (don't bother to check
    // output, the known answer tests already do that).
    EXPECT_EQ(Status::Success(),
              Encrypt(CreateAesCtrAlgorithm(counter, kCounterLengthBits), key,
                      input_16, &output));

    // Encrypting/Decrypting 17 however should fail.
    EXPECT_EQ(Status::ErrorAesCtrInputTooLongCounterRepeated(),
              Encrypt(CreateAesCtrAlgorithm(counter, kCounterLengthBits), key,
                      input_17, &output));
    EXPECT_EQ(Status::ErrorAesCtrInputTooLongCounterRepeated(),
              Decrypt(CreateAesCtrAlgorithm(counter, kCounterLengthBits), key,
                      input_17, &output));
  }
}

}  // namespace

}  // namespace webcrypto
