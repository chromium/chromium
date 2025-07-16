// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/kdf.h"

#include <array>
#include <string>

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using KDFTest = ::testing::Test;

TEST(KDFTest, Pbkdf2HmacSha1KnownAnswers) {
  struct TestCase {
    std::string password;
    std::string salt;
    crypto::kdf::Pbkdf2HmacSha1Params params;
    size_t len;
    const char* result;
  };

  // RFC 6070 test vectors:
  constexpr auto cases = std::to_array<TestCase>({
      {"password", "salt", {1}, 20, "0c60c80f961f0e71f3a9b524af6012062fe037a6"},
      {"password", "salt", {2}, 20, "ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957"},
  });

  for (const auto& c : cases) {
    std::vector<uint8_t> key(c.len);
    crypto::kdf::DeriveKeyPbkdf2HmacSha1(
        c.params, base::as_byte_span(c.password), base::as_byte_span(c.salt),
        key, crypto::SubtlePassKey::ForTesting());

    std::vector<uint8_t> result_bytes(c.len);
    ASSERT_TRUE(base::HexStringToSpan(c.result, result_bytes));
    EXPECT_EQ(key, result_bytes);
  }
}

TEST(KDFTest, ScryptKnownAnswers) {
  struct TestCase {
    std::string password;
    std::string salt;
    crypto::kdf::ScryptParams params;
    size_t len;
    const char* result;
  };

  // RFC 7914 test vectors - note that RFC 7914 does not specify
  // max_memory_bytes so we just pass 0 here and let BoringSSL figure it out for
  // us.
  constexpr auto cases = std::to_array<TestCase>({
      {"password",
       "NaCl",
       {.cost = 1024, .block_size = 8, .parallelization = 16},
       64,
       "fdbabe1c9d3472007856e7190d01e9fe7c6ad7cbc8237830e77376634b373162"
       "2eaf30d92e22a3886ff109279d9830dac727afb94a83ee6d8360cbdfa2cc0640"},
  });

  for (const auto& c : cases) {
    std::vector<uint8_t> key(c.len);
    crypto::kdf::DeriveKeyScrypt(c.params, base::as_byte_span(c.password),
                                 base::as_byte_span(c.salt), key,
                                 crypto::SubtlePassKey::ForTesting());

    std::vector<uint8_t> result_bytes(c.len);
    ASSERT_TRUE(base::HexStringToSpan(c.result, result_bytes));
    EXPECT_EQ(key, result_bytes);
  }
}

TEST(KDFTest, InvalidScryptParameters) {
  constexpr auto cases = std::to_array<crypto::kdf::ScryptParams>({
      // cost parameter is not a power of 2
      {.cost = 1023, .block_size = 8, .parallelization = 16},
      // TODO: others, after we document the exact constraints
  });

  for (const auto& c : cases) {
    std::vector<uint8_t> key(64);
    EXPECT_DEATH_IF_SUPPORTED(
        crypto::kdf::DeriveKeyScrypt(c, base::as_byte_span("password"),
                                     base::as_byte_span("NaCl"), key,
                                     crypto::SubtlePassKey::ForTesting()),
        "");
  }
}

TEST(KDFTest, HkdfSha256KnownAnswer) {
  // RFC 5869, Appendix A, Test Case 1
  // clang-format off
  constexpr auto kSecret = std::to_array<uint8_t>({
    0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
    0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
  });
  constexpr auto kSalt = std::to_array<uint8_t>({
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c,
  });
  constexpr auto kInfo = std::to_array<uint8_t>({
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9,
  });
  constexpr auto kExpected = std::to_array<uint8_t>({
    0x3c, 0xb2, 0x5f, 0x25, 0xfa, 0xac, 0xd5, 0x7a, 0x90, 0x43, 0x4f,
    0x64, 0xd0, 0x36, 0x2f, 0x2a, 0x2d, 0x2d, 0x0a, 0x90, 0xcf, 0x1a,
    0x5a, 0x4c, 0x5d, 0xb0, 0x2d, 0x56, 0xec, 0xc4, 0xc5, 0xbf, 0x34,
    0x00, 0x72, 0x08, 0xd5, 0xb8, 0x87, 0x18, 0x58, 0x65
  });
  // clang-format on

  {
    std::array<uint8_t, std::size(kExpected)> out;
    crypto::kdf::Hkdf(crypto::hash::kSha256, kSecret, kSalt, kInfo, out);
    EXPECT_EQ(kExpected, out);
  }

  {
    auto out = crypto::kdf::Hkdf<std::size(kExpected)>(crypto::hash::kSha256,
                                                       kSecret, kSalt, kInfo);
    EXPECT_EQ(kExpected, out);
  }
}
