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
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
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

auto AesGcmAlgorithm() {
  return fuzztest::Map(
      [](std::vector<unsigned char> iv, bool has_additional_data,
         std::vector<unsigned char> additional_data, bool has_tag_length_bits,
         unsigned char tag_length_bits) {
        return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
            blink::kWebCryptoAlgorithmIdAesGcm,
            new blink::WebCryptoAesGcmParams(
                std::move(iv), has_additional_data, std::move(additional_data),
                has_tag_length_bits, tag_length_bits));
      },
      fuzztest::Arbitrary<std::vector<unsigned char>>(),
      fuzztest::Arbitrary<bool>(),
      fuzztest::Arbitrary<std::vector<unsigned char>>(),
      fuzztest::Arbitrary<bool>(), fuzztest::Arbitrary<unsigned char>());
}

auto AesKwAlgorithm() {
  return fuzztest::Map([]() {
    return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
        blink::kWebCryptoAlgorithmIdAesKw, nullptr);
  });
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

auto X25519Algorithm() {
  return fuzztest::Map([]() {
    return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
        blink::kWebCryptoAlgorithmIdX25519, nullptr);
  });
}

auto Ed25519Algorithm() {
  return fuzztest::Map([]() {
    return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
        blink::kWebCryptoAlgorithmIdEd25519, nullptr);
  });
}

auto Pbkdf2Algorithm(fuzztest::Domain<blink::WebCryptoAlgorithm> entry_domain) {
  return fuzztest::Map(
      [](blink::WebCryptoAlgorithm algo, std::vector<unsigned char> salt,
         unsigned iteration) {
        return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
            blink::kWebCryptoAlgorithmIdPbkdf2,
            new blink::WebCryptoPbkdf2Params(algo, std::move(salt), iteration));
      },
      entry_domain, fuzztest::Arbitrary<std::vector<unsigned char>>(),
      fuzztest::Arbitrary<unsigned>());
}

auto HkdfAlgorithm(fuzztest::Domain<blink::WebCryptoAlgorithm> entry_domain) {
  return fuzztest::Map(
      [](blink::WebCryptoAlgorithm algo, std::vector<unsigned char> salt,
         std::vector<unsigned char> info) {
        return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
            blink::kWebCryptoAlgorithmIdHkdf,
            new blink::WebCryptoHkdfParams(algo, std::move(salt),
                                           std::move(info)));
      },
      entry_domain, fuzztest::Arbitrary<std::vector<unsigned char>>(),
      fuzztest::Arbitrary<std::vector<unsigned char>>());
}

auto RsaPssAlgorithm() {
  return fuzztest::Map(
      [](unsigned salt_length_bytes) {
        return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
            blink::kWebCryptoAlgorithmIdRsaPss,
            new blink::WebCryptoRsaPssParams(salt_length_bytes));
      },
      fuzztest::Arbitrary<unsigned>());
}

auto RsaOaepAlgorithm() {
  return fuzztest::Map(
      [](bool has_label, std::vector<unsigned char> label) {
        return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
            blink::kWebCryptoAlgorithmIdRsaOaep,
            new blink::WebCryptoRsaOaepParams(has_label, std::move(label)));
      },
      fuzztest::Arbitrary<bool>(),
      fuzztest::Arbitrary<std::vector<unsigned char>>());
}

auto Sha1Algorithm() {
  return fuzztest::Map([]() {
    return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
        blink::kWebCryptoAlgorithmIdSha1, nullptr);
  });
}

auto Sha256Algorithm() {
  return fuzztest::Map([]() {
    return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
        blink::kWebCryptoAlgorithmIdSha256, nullptr);
  });
}

auto Sha384Algorithm() {
  return fuzztest::Map([]() {
    return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
        blink::kWebCryptoAlgorithmIdSha384, nullptr);
  });
}

auto Sha512Algorithm() {
  return fuzztest::Map([]() {
    return blink::WebCryptoAlgorithm::AdoptParamsAndCreate(
        blink::kWebCryptoAlgorithmIdSha512, nullptr);
  });
}

auto AnyAlgorithm() {
  fuzztest::DomainBuilder builder;
  builder.Set<blink::WebCryptoAlgorithm>(
      "algorithm",
      fuzztest::OneOf(
          AesCbcAlgorithm(), AesCtrAlgorithm(), AesGcmAlgorithm(),
          AesKwAlgorithm(),
          HmacAlgorithm(builder.Get<blink::WebCryptoAlgorithm>("algorithm")),
          Pbkdf2Algorithm(builder.Get<blink::WebCryptoAlgorithm>("algorithm")),
          HkdfAlgorithm(builder.Get<blink::WebCryptoAlgorithm>("algorithm")),
          X25519Algorithm(), Ed25519Algorithm(), RsaPssAlgorithm(),
          RsaOaepAlgorithm(), Sha1Algorithm(), Sha256Algorithm(),
          Sha384Algorithm(), Sha512Algorithm()));
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

static void DigestFuzzer(blink::WebCryptoAlgorithm algo,
                         base::span<uint8_t> data) {
  std::vector<uint8_t> buffer;
  webcrypto::Digest(algo, data, &buffer);
}

static void SignFuzzer(blink::WebCryptoAlgorithm algo,
                       blink::WebCryptoKeyUsage key_usage,
                       base::span<uint8_t> key_data,
                       base::span<uint8_t> data) {
  blink::WebCryptoKey key;
  auto status = webcrypto::ImportKey(blink::kWebCryptoKeyFormatRaw, key_data,
                                     algo, true, key_usage, &key);
  if (!status.IsSuccess() || key.Algorithm().IsNull()) {
    return;
  }
  std::vector<uint8_t> buffer;
  webcrypto::Sign(algo, key, data, &buffer);
}

static void VerifyFuzzer(blink::WebCryptoAlgorithm algo,
                         blink::WebCryptoKeyUsage key_usage,
                         base::span<uint8_t> key_data,
                         base::span<uint8_t> signature,
                         base::span<uint8_t> data) {
  blink::WebCryptoKey key;
  auto status = webcrypto::ImportKey(blink::kWebCryptoKeyFormatRaw, key_data,
                                     algo, true, key_usage, &key);
  if (!status.IsSuccess() || key.Algorithm().IsNull()) {
    return;
  }
  bool match;
  webcrypto::Verify(algo, key, signature, data, &match);
}

static void DeriveBitsFuzzer(blink::WebCryptoAlgorithm algo,
                             blink::WebCryptoKeyUsage key_usage,
                             base::span<uint8_t> key_data,
                             unsigned int length_bits) {
  blink::WebCryptoKey key;
  auto status = webcrypto::ImportKey(blink::kWebCryptoKeyFormatRaw, key_data,
                                     algo, true, key_usage, &key);
  if (!status.IsSuccess() || key.Algorithm().IsNull()) {
    return;
  }
  std::vector<uint8_t> derived_bytes;
  webcrypto::DeriveBits(algo, key, length_bits, &derived_bytes);
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

FUZZ_TEST(WebCryptoFuzzer, DigestFuzzer)
    .WithDomains(AnyAlgorithm(), fuzztest::Arbitrary<std::vector<uint8_t>>());

FUZZ_TEST(WebCryptoFuzzer, SignFuzzer)
    .WithDomains(AnyAlgorithm(),
                 AnyKeyUsage(),
                 fuzztest::Arbitrary<std::vector<uint8_t>>(),
                 fuzztest::Arbitrary<std::vector<uint8_t>>());

FUZZ_TEST(WebCryptoFuzzer, VerifyFuzzer)
    .WithDomains(AnyAlgorithm(),
                 AnyKeyUsage(),
                 fuzztest::Arbitrary<std::vector<uint8_t>>(),
                 fuzztest::Arbitrary<std::vector<uint8_t>>(),
                 fuzztest::Arbitrary<std::vector<uint8_t>>());

FUZZ_TEST(WebCryptoFuzzer, DeriveBitsFuzzer)
    .WithDomains(AnyAlgorithm(),
                 AnyKeyUsage(),
                 fuzztest::Arbitrary<std::vector<uint8_t>>(),
                 fuzztest::Arbitrary<unsigned int>());
