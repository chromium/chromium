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

TEST(HashTest, Sha384) {
  // FIPS 180-4 "SHA384ShortMsg" test vector:
  TestCase c = {
      .message = "e1bb967b5d379a4aa39050274d09bd93",
      .digest =
          "3b04f96965ad2fbabd4df25d5d8c95589d069c312ee48539090b2d7b495d2446c31e"
          "b2b8f8ffb3012bdce065323d9f48",
  };

  std::vector<uint8_t> message;
  std::array<uint8_t, 48> digest;
  PrepareTestCase(c, &message, digest);

  std::vector<uint8_t> computed_digest(
      crypto::hash::DigestSizeForHashKind(crypto::hash::kSha384));
  crypto::hash::Hash(crypto::hash::kSha384, base::as_byte_span(message),
                     computed_digest);
  EXPECT_EQ(base::as_byte_span(digest), base::as_byte_span(computed_digest));
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

TEST(HashTest, StringViewHash) {
  const std::array<uint8_t, crypto::hash::kSha256Size> hash{
      0xdf, 0xfd, 0x60, 0x21, 0xbb, 0x2b, 0xd5, 0xb0, 0xaf, 0x67, 0x62,
      0x90, 0x80, 0x9e, 0xc3, 0xa5, 0x31, 0x91, 0xdd, 0x81, 0xc7, 0xf7,
      0x0a, 0x4b, 0x28, 0x68, 0x8a, 0x36, 0x21, 0x82, 0x98, 0x6f};
  EXPECT_EQ(hash, crypto::hash::Sha256("Hello, World!"));
}
