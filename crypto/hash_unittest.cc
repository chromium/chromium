// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/hash.h"

#include <array>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

struct TestCase {
  const char* message;
  const char* digest;
};

void PrepareTestCase(const TestCase& c,
                     std::vector<uint8_t>* message,
                     base::span<uint8_t> digest) {
  CHECK(base::HexStringToBytes(c.message, message));
  CHECK(base::HexStringToSpan(c.digest, digest));
}

TEST(HashTest, Sha1) {
  const auto cases = std::to_array<TestCase>(
      {// FIPS 180-4 "SHA1ShortMsg" test vector:
       {
           .message = "3552694cdf663fd94b224747ac406aaf",
           .digest = "a150de927454202d94e656de4c7c0ca691de955d",
       }});

  for (const auto& c : cases) {
    std::vector<uint8_t> message;
    std::array<uint8_t, 20> digest;

    PrepareTestCase(c, &message, digest);
    auto computed_digest = crypto::hash::Sha1(base::as_byte_span(message));
    EXPECT_EQ(digest, computed_digest);
  }
}

TEST(HashTest, Sha256) {
  const auto cases = std::to_array<TestCase>(
      {// FIPS 180-4 "SHA256ShortMsg" test vector:
       {
           .message = "0a27847cdc98bd6f62220b046edd762b",
           .digest = "80c25ec1600587e7f28b18b1b18e3cdc89928e39cab3bc25e4d4a4c13"
                     "9bcedc4",
       }});

  for (const auto& c : cases) {
    std::vector<uint8_t> message;
    std::array<uint8_t, 32> digest;

    PrepareTestCase(c, &message, digest);
    auto computed_digest = crypto::hash::Sha256(base::as_byte_span(message));
    EXPECT_EQ(digest, computed_digest);
  }
}

TEST(HashTest, Sha512) {
  const auto cases = std::to_array<TestCase>(
      {// FIPS 180-4 "SHA512ShortMsg" test vector:
       {
           .message = "cd67bd4054aaa3baa0db178ce232fd5a",
           .digest = "0d8521f8f2f3900332d1a1a55c60ba81d04d28dfe8c504b6328ae7879"
                     "25fe018"
                     "8f2ba91c3a9f0c1653c4bf0ada356455ea36fd31f8e73e3951cad4ebb"
                     "a8c6e04",
       }});

  for (const auto& c : cases) {
    std::vector<uint8_t> message;
    std::array<uint8_t, 64> digest;

    PrepareTestCase(c, &message, digest);
    auto computed_digest = crypto::hash::Sha512(base::as_byte_span(message));
    EXPECT_EQ(digest, computed_digest);
  }
}

TEST(HashTest, WrongDigestSizeDies) {
  std::array<uint8_t, 16> small_digest;
  std::array<uint8_t, 128> big_digest;
  std::array<uint8_t, 16> input;

  EXPECT_DEATH_IF_SUPPORTED(
      crypto::hash::Hash(crypto::hash::HashKind::kSha256, input, small_digest),
      "");
  EXPECT_DEATH_IF_SUPPORTED(
      crypto::hash::Hash(crypto::hash::HashKind::kSha256, input, big_digest),
      "");
}
