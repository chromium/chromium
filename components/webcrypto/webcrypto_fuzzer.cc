// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_crypto_algorithm.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

auto AnyKeyUsage() {
  return fuzztest::BitFlagCombinationOf<blink::WebCryptoKeyUsage>(
      {blink::kWebCryptoKeyUsageEncrypt, blink::kWebCryptoKeyUsageDecrypt,
       blink::kWebCryptoKeyUsageSign, blink::kWebCryptoKeyUsageVerify,
       blink::kWebCryptoKeyUsageDeriveKey, blink::kWebCryptoKeyUsageWrapKey,
       blink::kWebCryptoKeyUsageUnwrapKey,
       blink::kWebCryptoKeyUsageDeriveBits});
}

auto AesCbcAlgorithm() {
  return fuzztest::Map(
      [](std::vector<unsigned char> param) {
        return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
            blink::kWebCryptoAlgorithmIdAesCbc,
            new blink::WebCryptoAesCbcParams(std::move(param)));
      },
      fuzztest::Arbitrary<std::vector<unsigned char>>());
}

auto AesCtrAlgorithm() {
  return fuzztest::Map(
      [](unsigned char length_bits, std::vector<unsigned char> counter) {
        return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
            blink::kWebCryptoAlgorithmIdAesCtr,
            new blink::WebCryptoAesCtrParams(length_bits, std::move(counter)));
      },
      fuzztest::Arbitrary<unsigned char>(),
      fuzztest::Arbitrary<std::vector<unsigned char>>());
}

auto AesAlgorithm() {
  return fuzztest::Map(
      [](unsigned char length_bits, std::vector<unsigned char> counter) {
        return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
            blink::kWebCryptoAlgorithmIdAesCtr,
            new blink::WebCryptoAesCtrParams(length_bits, std::move(counter)));
      },
      fuzztest::Arbitrary<unsigned char>(),
      fuzztest::Arbitrary<std::vector<unsigned char>>());
}

auto HmacAlgorithm(fuzztest::Domain<blink::WebCryptoAlgorithm> entry_domain) {
  return fuzztest::Map(
      [](blink::WebCryptoAlgorithm algo, bool has_length,
         unsigned length_bits) {
        return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
            blink::kWebCryptoAlgorithmIdHmac,
            new blink::WebCryptoHmacImportParams(algo, has_length,
                                                 length_bits));
      },
      entry_domain, fuzztest::Arbitrary<bool>(),
      fuzztest::Arbitrary<unsigned>());
}

auto AnyAlgorithm() {
  fuzztest::DomainBuilder builder;
  builder.Set<blink::WebCryptoAlgorithm>(
      "algorithm",
      fuzztest::OneOf(
          AesCbcAlgorithm(), AesCtrAlgorithm(), AesAlgorithm(),
          HmacAlgorithm(builder.Get<blink::WebCryptoAlgorithm>("algorithm"))));
  return std::move(builder).Finalize<blink::WebCryptoAlgorithm>("algorithm");
}

static void ImportKeyFuzzer(blink::WebCryptoAlgorithm algo,
                            blink::WebCryptoKeyUsage key_usage,
                            base::span<uint8_t> key_data) {
  blink::WebCryptoKey key;
  auto status = webcrypto::ImportKey(blink::kWebCryptoKeyFormatRaw, key_data,
                                     algo, true, key_usage, &key);
}

static void EncryptFuzzer(blink::WebCryptoAlgorithm algo,
                          blink::WebCryptoKeyUsage key_usage,
                          base::span<uint8_t> key_data,
                          base::span<uint8_t> data) {
  blink::WebCryptoKey key;
  auto status = webcrypto::ImportKey(blink::kWebCryptoKeyFormatRaw, key_data,
                                     algo, true, key_usage, &key);
  if (!status.IsSuccess()) {
    return;
  }
  std::vector<uint8_t> buffer;
  webcrypto::Encrypt(algo, key, data, &buffer);
}

static void DecryptFuzzer(blink::WebCryptoAlgorithm algo,
                          blink::WebCryptoKeyUsage key_usage,
                          base::span<uint8_t> key_data,
                          base::span<uint8_t> data) {
  blink::WebCryptoKey key;
  auto status = webcrypto::ImportKey(blink::kWebCryptoKeyFormatRaw, key_data,
                                     algo, true, key_usage, &key);
  if (!status.IsSuccess()) {
    return;
  }
  std::vector<uint8_t> buffer;
  webcrypto::Decrypt(algo, key, data, &buffer);
}

FUZZ_TEST(WebCryptoFuzzer, ImportKeyFuzzer)
    .WithDomains(AnyAlgorithm(),
                 AnyKeyUsage(),
                 fuzztest::Arbitrary<std::vector<uint8_t>>());

FUZZ_TEST(WebCryptoFuzzer, EncryptFuzzer)
    .WithDomains(AnyAlgorithm(),
                 AnyKeyUsage(),
                 fuzztest::Arbitrary<std::vector<uint8_t>>(),
                 fuzztest::Arbitrary<std::vector<uint8_t>>());

FUZZ_TEST(WebCryptoFuzzer, DecryptFuzzer)
    .WithDomains(AnyAlgorithm(),
                 AnyKeyUsage(),
                 fuzztest::Arbitrary<std::vector<uint8_t>>(),
                 fuzztest::Arbitrary<std::vector<uint8_t>>());
