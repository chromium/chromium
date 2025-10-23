// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/hmac.h"

#include <stddef.h>
#include <string.h>

#include <array>
#include <string>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(HMACTest, OneShotSha1) {
  // RFC 2202 test case 3:
  std::vector<uint8_t> key(20, 0xaa);
  std::vector<uint8_t> data(50, 0xdd);
  std::vector<uint8_t> expected;
  CHECK(base::HexStringToBytes("125d7342b9ac11cd91a39af48aa17b4f63f175d3",
                               &expected));

  // Old API signing:
  {
    crypto::HMAC hmac(crypto::HMAC::SHA1);
    ASSERT_TRUE(hmac.Init(key));
    std::array<uint8_t, crypto::hash::kSha1Size> result;
    EXPECT_TRUE(hmac.Sign(data, result));
    EXPECT_EQ(base::as_byte_span(expected), result);
  }

  // Old API verification:
  {
    crypto::HMAC hmac(crypto::HMAC::SHA1);
    ASSERT_TRUE(hmac.Init(key));
    EXPECT_TRUE(hmac.Verify(data, expected));
  }

  auto result = crypto::hmac::SignSha1(key, data);
  EXPECT_EQ(base::as_byte_span(result), base::as_byte_span(expected));
  EXPECT_TRUE(crypto::hmac::VerifySha1(key, data, result));
  result[0] ^= 0x01;
  EXPECT_FALSE(crypto::hmac::VerifySha1(key, data, result));
}

TEST(HMACTest, OneShotSha256) {
  // RFC 4231 test case 3:
  std::vector<uint8_t> key(20, 0xaa);
  std::vector<uint8_t> data(50, 0xdd);
  std::vector<uint8_t> expected;
  CHECK(base::HexStringToBytes(
      "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe",
      &expected));

  // Old API signing:
  {
    crypto::HMAC hmac(crypto::HMAC::SHA256);
    ASSERT_TRUE(hmac.Init(key));
    std::array<uint8_t, crypto::hash::kSha256Size> result;
    EXPECT_TRUE(hmac.Sign(data, result));
    EXPECT_EQ(base::as_byte_span(expected), result);
  }

  // Old API verification:
  {
    crypto::HMAC hmac(crypto::HMAC::SHA256);
    ASSERT_TRUE(hmac.Init(key));
    EXPECT_TRUE(hmac.Verify(data, expected));
  }

  auto result = crypto::hmac::SignSha256(key, data);
  EXPECT_EQ(base::as_byte_span(result), base::as_byte_span(expected));
  EXPECT_TRUE(crypto::hmac::VerifySha256(key, data, result));
  result[0] ^= 0x01;
  EXPECT_FALSE(crypto::hmac::VerifySha256(key, data, result));
}

TEST(HMACTest, OneShotSha384) {
  // RFC 4231 test case 3:
  std::vector<uint8_t> key(20, 0xaa);
  std::vector<uint8_t> data(50, 0xdd);
  std::vector<uint8_t> expected;
  CHECK(
      base::HexStringToBytes("88062608d3e6ad8a0aa2ace014c8a86f0aa635d947ac9febe"
                             "83ef4e55966144b2a5ab39dc13814b94e3ab6e101a34f27",
                             &expected));

  std::array<uint8_t, crypto::hash::kSha384Size> result;
  crypto::hmac::Sign(crypto::hash::kSha384, key, data, result);
  EXPECT_EQ(base::as_byte_span(result), base::as_byte_span(expected));
  EXPECT_TRUE(crypto::hmac::Verify(crypto::hash::kSha384, key, data, result));
  result[0] ^= 0x01;
  EXPECT_FALSE(crypto::hmac::Verify(crypto::hash::kSha384, key, data, result));
}

TEST(HMACTest, OneShotSha512) {
  // RFC 4231 test case 3:
  std::vector<uint8_t> key(20, 0xaa);
  std::vector<uint8_t> data(50, 0xdd);
  std::vector<uint8_t> expected;
  CHECK(base::HexStringToBytes(
      "fa73b0089d56a284efb0f0756c890be9b1b5dbdd8ee81a3655f83e33b2279d39"
      "bf3e848279a722c806b485a47e67c807b946a337bee8942674278859e13292fb",
      &expected));

  auto result = crypto::hmac::SignSha512(key, data);
  EXPECT_EQ(base::as_byte_span(result), base::as_byte_span(expected));
  EXPECT_TRUE(crypto::hmac::VerifySha512(key, data, result));
  result[0] ^= 0x01;
  EXPECT_FALSE(crypto::hmac::VerifySha512(key, data, result));
}

TEST(HMACTest, OneShotWrongLengthDies) {
  std::array<uint8_t, 32> key;
  std::array<uint8_t, 32> data;
  std::array<uint8_t, 16> small_hmac;
  std::array<uint8_t, 128> big_hmac;

  EXPECT_DEATH_IF_SUPPORTED(crypto::hmac::Sign(crypto::hash::HashKind::kSha256,
                                               key, data, small_hmac),
                            "");
  EXPECT_DEATH_IF_SUPPORTED(
      crypto::hmac::Sign(crypto::hash::HashKind::kSha256, key, data, big_hmac),
      "");

  EXPECT_DEATH_IF_SUPPORTED(
      (void)crypto::hmac::Verify(crypto::hash::HashKind::kSha256, key, data,
                                 small_hmac),
      "");
  EXPECT_DEATH_IF_SUPPORTED(
      (void)crypto::hmac::Verify(crypto::hash::HashKind::kSha256, key, data,
                                 big_hmac),
      "");
}

TEST(HMACTest, StreamingSha512) {
  // RFC 4231 test case 3:
  std::vector<uint8_t> key(20, 0xaa);
  std::vector<uint8_t> data1(32, 0xdd);
  std::vector<uint8_t> data2(18, 0xdd);
  std::vector<uint8_t> expected;
  CHECK(base::HexStringToBytes(
      "fa73b0089d56a284efb0f0756c890be9b1b5dbdd8ee81a3655f83e33b2279d39"
      "bf3e848279a722c806b485a47e67c807b946a337bee8942674278859e13292fb",
      &expected));

  crypto::hmac::HmacSigner signer(crypto::hash::kSha512, key);
  signer.Update(data1);
  signer.Update(data2);
  auto result = signer.Finish();
  EXPECT_EQ(base::as_byte_span(result), base::as_byte_span(expected));

  crypto::hmac::HmacVerifier verifier(crypto::hash::kSha512, key);
  verifier.Update(data1);
  verifier.Update(data2);
  EXPECT_TRUE(verifier.Finish(result));
}
