// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/aes_cbc.h"

#include <cstring>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(AesCbcTests, KnownAnswers) {
  struct TestCase {
    const char* key;
    const char* iv;
    const char* plaintext;
    const char* ciphertext;
  };

  constexpr auto cases = std::to_array<TestCase>({
      // SP800-38a F.2.1, with a PKCS#5 padding block appended to it.
      {
          .key = "2b7e151628aed2a6abf7158809cf4f3c",
          .iv = "000102030405060708090a0b0c0d0e0f",
          .plaintext = "6bc1bee22e409f96e93d7e117393172a"
                       "ae2d8a571e03ac9c9eb76fac45af8e51"
                       "30c81c46a35ce411e5fbc1191a0a52ef"
                       "f69f2445df4f9b17ad2b417be66c3710",
          .ciphertext = "7649abac8119b246cee98e9b12e9197d"
                        "5086cb9b507219ee95db113a917678b2"
                        "73bed6b8e3c1743b7116e69e22229516"
                        "3ff1caa1681fac09120eca307586e1a7"
                        "8cb82807230e1321d3fae00d18cc2012",
      },

      // Modification of SP800-38a F.2.6: stripped off a bit of plaintext, added
      // padding block.
      {.key =
           "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4",
       .iv = "000102030405060708090a0b0c0d0e0f",
       .plaintext = "6bc1bee22e409f96e93d7e117393172a"
                    "ae2d8a571e03ac9c9eb76fac45af8e51"
                    "30c81c46a35ce411e5fbc1191a0a52ef"
                    "f69f2445df4f9b17ad2b417be6",
       .ciphertext = "f58c4c04d6e5f1ba779eabfb5f7bfbd6"
                     "9cfc4e967edb808d679f777bc6702c7d"
                     "39f23369a9d9bacfa530e26304231461"
                     "c9aaf02a6a54e9e242ccbf48c59daca6"},
  });

  for (const auto& c : cases) {
    std::vector<uint8_t> key;
    CHECK(base::HexStringToBytes(c.key, &key));

    std::array<uint8_t, crypto::aes_cbc::kBlockSize> iv;
    CHECK(base::HexStringToSpan(c.iv, iv));

    std::vector<uint8_t> plaintext;
    CHECK(base::HexStringToBytes(c.plaintext, &plaintext));

    std::vector<uint8_t> ciphertext;
    CHECK(base::HexStringToBytes(c.ciphertext, &ciphertext));

    std::vector<uint8_t> computed_ciphertext =
        crypto::aes_cbc::Encrypt(key, iv, plaintext);
    EXPECT_EQ(computed_ciphertext, ciphertext);
    std::optional<std::vector<uint8_t>> computed_plaintext =
        crypto::aes_cbc::Decrypt(key, iv, computed_ciphertext);
    ASSERT_TRUE(computed_plaintext.has_value());
    EXPECT_EQ(*computed_plaintext, plaintext);
  }
}

TEST(AesCbcTests, DecryptFailure) {
  std::vector<uint8_t> key;
  std::array<uint8_t, crypto::aes_cbc::kBlockSize> iv;
  std::vector<uint8_t> plaintext;

  CHECK(base::HexStringToBytes("2b7e151628aed2a6abf7158809cf4f3c", &key));
  CHECK(base::HexStringToSpan("000102030405060708090a0b0c0d0e0f", iv));
  CHECK(
      base::HexStringToBytes("6bc1bee22e409f96e93d7e117393172a"
                             "ae2d8a571e03ac9c9eb76fac45af8e51"
                             "30c81c46a35ce411e5fbc1191a0a52ef"
                             "f69f2445df4f9b17ad2b417be66c3710",
                             &plaintext));

  std::vector<uint8_t> ciphertext =
      crypto::aes_cbc::Encrypt(key, iv, plaintext);
  ciphertext[ciphertext.size() - 1] ^= 0x01;
  std::optional<std::vector<uint8_t>> result =
      crypto::aes_cbc::Decrypt(key, iv, ciphertext);
  ASSERT_FALSE(result.has_value());
}

}  // namespace
