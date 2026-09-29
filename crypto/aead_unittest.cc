// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/aead.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/strings/string_view_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const crypto::aead::Algorithm kAllAlgorithms[]{
    crypto::aead::AES_128_CTR_HMAC_SHA256,
    crypto::aead::AES_128_GCM,
    crypto::aead::AES_256_GCM,
    crypto::aead::AES_256_GCM_SIV,
    crypto::aead::CHACHA20_POLY1305,
};

std::vector<uint8_t> FixedKeyFor(crypto::aead::Algorithm algo, uint8_t fill) {
  return std::vector<uint8_t>(crypto::aead::KeySizeFor(algo), fill);
}

std::vector<uint8_t> FixedNonceFor(crypto::aead::Algorithm algo, uint8_t fill) {
  return std::vector<uint8_t>(crypto::aead::NonceSizeFor(algo), fill);
}

class AeadTest : public testing::TestWithParam<crypto::aead::Algorithm> {};

INSTANTIATE_TEST_SUITE_P(All, AeadTest, testing::ValuesIn(kAllAlgorithms));

TEST_P(AeadTest, SealOpenString) {
  const crypto::aead::Algorithm alg = GetParam();
  crypto::Aead aead(alg, FixedKeyFor(alg, 0));
  const auto nonce = FixedNonceFor(alg, 0);
  const std::string_view plaintext = "this is the plaintext";
  const std::string_view ad = "this is the additional data";
  std::string ciphertext;
  EXPECT_TRUE(
      aead.Seal(plaintext, base::as_string_view(nonce), ad, &ciphertext));
  EXPECT_GT(ciphertext.size(), plaintext.size());

  std::string decrypted;
  EXPECT_TRUE(
      aead.Open(ciphertext, base::as_string_view(nonce), ad, &decrypted));

  EXPECT_EQ(plaintext, decrypted);
}

TEST_P(AeadTest, SealOpenSpan) {
  const crypto::aead::Algorithm alg = GetParam();
  crypto::Aead aead(alg, FixedKeyFor(alg, 0));
  const auto nonce = FixedNonceFor(alg, 0);
  constexpr auto plaintext = std::to_array<uint8_t>({0x01, 0x23, 0x45, 0x67});
  constexpr auto ad = std::to_array<uint8_t>({0x89, 0xab, 0xcd, 0xef});
  std::vector<uint8_t> ciphertext = aead.Seal(plaintext, nonce, ad);
  EXPECT_GT(ciphertext.size(), plaintext.size());

  std::optional<std::vector<uint8_t>> decrypted =
      aead.Open(ciphertext, nonce, ad);
  ASSERT_TRUE(decrypted);
  ASSERT_EQ(base::span(*decrypted), base::span(plaintext));
}

TEST_P(AeadTest, SealOpenWrongKey) {
  const crypto::aead::Algorithm alg = GetParam();
  crypto::Aead aead(alg, FixedKeyFor(alg, 0));
  crypto::Aead aead_wrong_key(alg, FixedKeyFor(alg, 1));

  const auto nonce = FixedNonceFor(alg, 0);
  constexpr auto plaintext = std::to_array<uint8_t>({0x01, 0x23, 0x45, 0x67});
  constexpr auto ad = std::to_array<uint8_t>({0x89, 0xab, 0xcd, 0xef});
  const auto ciphertext = aead.Seal(plaintext, nonce, ad);
  EXPECT_GT(ciphertext.size(), plaintext.size());

  EXPECT_FALSE(aead_wrong_key.Open(ciphertext, nonce, ad));
}

TEST_P(AeadTest, SealOpenTooShortKey) {
  std::array<uint8_t, 1> key;
  crypto::Aead aead(GetParam(), key);

  std::string nonce(aead.NonceLength(), 0);
  std::string plaintext("this is the plaintext");
  std::string ad("this is the additional data");
  std::string ciphertext;
  EXPECT_FALSE(aead.Seal(plaintext, nonce, ad, &ciphertext));
}

TEST_P(AeadTest, OneShotRoundTrips) {
  crypto::aead::Algorithm alg = GetParam();

  constexpr auto plaintext = std::to_array<uint8_t>({0x01, 0x23, 0x45, 0x67});
  constexpr auto ad = std::to_array<uint8_t>({0x89, 0xab, 0xcd, 0xef});
  std::vector<uint8_t> key(crypto::aead::KeySizeFor(alg));
  std::vector<uint8_t> nonce(crypto::aead::NonceSizeFor(alg));

  const auto ciphertext = crypto::aead::Seal(alg, key, plaintext, nonce, ad);
  const auto recovered = crypto::aead::Open(alg, key, ciphertext, nonce, ad);

  ASSERT_TRUE(recovered);
  EXPECT_EQ(base::as_byte_span(*recovered), base::as_byte_span(plaintext));
}

}  // namespace
