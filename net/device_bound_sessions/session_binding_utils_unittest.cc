// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_binding_utils.h"

#include <optional>
#include <string_view>

#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "base/value_iterators.h"
#include "base/values.h"
#include "crypto/signature_verifier.h"
#include "net/device_bound_sessions/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

namespace {

base::Value Base64UrlEncodedJsonToValue(std::string_view input) {
  std::string json;
  EXPECT_TRUE(base::Base64UrlDecode(
      input, base::Base64UrlDecodePolicy::DISALLOW_PADDING, &json));
  std::optional<base::Value> result = base::JSONReader::Read(json);
  EXPECT_TRUE(result.has_value());
  return std::move(*result);
}

}  // namespace

TEST(SessionBindingUtilsTest, CreateKeyRegistrationHeaderAndPayload) {
  auto [spki, jwk] = GetRS256SpkiAndJwkForTesting();

  std::optional<std::string> result = CreateKeyRegistrationHeaderAndPayload(
      "test_challenge", GURL("https://accounts.example.test/RegisterKey"),
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256, spki,
      base::Time::UnixEpoch() + base::Days(200) + base::Milliseconds(123), "");
  ASSERT_TRUE(result.has_value());

  std::vector<std::string_view> header_and_payload = base::SplitStringPiece(
      *result, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(header_and_payload.size(), 2U);
  base::Value actual_header =
      Base64UrlEncodedJsonToValue(header_and_payload[0]);
  base::Value actual_payload =
      Base64UrlEncodedJsonToValue(header_and_payload[1]);

  base::Value::Dict expected_header =
      base::Value::Dict().Set("alg", "RS256").Set("typ", "jwt");
  base::Value::Dict expected_payload =
      base::Value::Dict()
          .Set("aud", "https://accounts.example.test/RegisterKey")
          .Set("jti", "test_challenge")
          .Set("iat", 17280000)
          .Set("key", base::JSONReader::Read(jwk).value())
          .Set("authorization", "");

  EXPECT_EQ(actual_header, expected_header);
  EXPECT_EQ(actual_payload, expected_payload);
}

TEST(SessionBindingUtilsTest,
     CreateKeyRegistrationHeaderAndPayloadWithNullAuth) {
  auto [spki, jwk] = GetRS256SpkiAndJwkForTesting();

  std::optional<std::string> result = CreateKeyRegistrationHeaderAndPayload(
      "test_challenge", GURL("https://accounts.example.test/RegisterKey"),
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256, spki,
      base::Time::UnixEpoch() + base::Days(200) + base::Milliseconds(123),
      /*authorization=*/std::nullopt);
  ASSERT_TRUE(result.has_value());

  std::vector<std::string_view> header_and_payload = base::SplitStringPiece(
      *result, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(header_and_payload.size(), 2U);
  base::Value actual_header =
      Base64UrlEncodedJsonToValue(header_and_payload[0]);
  base::Value actual_payload =
      Base64UrlEncodedJsonToValue(header_and_payload[1]);

  base::Value::Dict expected_header =
      base::Value::Dict().Set("alg", "RS256").Set("typ", "jwt");
  base::Value::Dict expected_payload =
      base::Value::Dict()
          .Set("aud", "https://accounts.example.test/RegisterKey")
          .Set("jti", "test_challenge")
          .Set("iat", 17280000)
          .Set("key", base::JSONReader::Read(jwk).value());

  EXPECT_EQ(actual_header, expected_header);
  EXPECT_EQ(actual_payload, expected_payload);
}

TEST(SessionBindingUtilsTest, AppendSignatureToHeaderAndPayload) {
  std::optional<std::string> result = AppendSignatureToHeaderAndPayload(
      "abc.efg",
      crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256,
      std::vector<uint8_t>({1, 2, 3}));
  EXPECT_EQ(result, "abc.efg.AQID");
}

TEST(SessionBindingUtilsTest,
     AppendSignatureToHeaderAndPayloadValidECDSASignature) {
  const std::vector<uint8_t> kDerSignature = {
      0x30, 0x45, 0x02, 0x20, 0x74, 0xa0, 0x6f, 0x6b, 0x2b, 0x0e, 0x82, 0x0e,
      0x03, 0x3b, 0x6e, 0x98, 0xfc, 0x89, 0x9c, 0xf3, 0x30, 0xb5, 0x56, 0xd3,
      0x29, 0x89, 0xb5, 0x82, 0x33, 0x5f, 0x9d, 0x97, 0xfb, 0x65, 0x64, 0x90,
      0x02, 0x21, 0x00, 0xbc, 0xb5, 0xee, 0x42, 0xe2, 0x5a, 0x87, 0xae, 0x21,
      0x18, 0xda, 0x7e, 0x68, 0x65, 0x30, 0xbe, 0xe5, 0x69, 0x3d, 0xc5, 0x5f,
      0xd5, 0x62, 0x45, 0x3e, 0x8d, 0x0b, 0x05, 0x1a, 0x33, 0x79, 0x8d};
  constexpr std::string_view kRawSignatureBase64UrlEncoded =
      "dKBvaysOgg4DO26Y_Imc8zC1VtMpibWCM1-dl_tlZJC8te5C4lqHriEY2n5oZTC-5Wk9xV_"
      "VYkU-jQsFGjN5jQ";

  std::optional<std::string> result = AppendSignatureToHeaderAndPayload(
      "abc.efg", crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
      kDerSignature);
  EXPECT_EQ(result, base::StrCat({"abc.efg.", kRawSignatureBase64UrlEncoded}));
}

TEST(SessionBindingUtilsTest,
     AppendSignatureToHeaderAndPayloadInvalidECDSASignature) {
  std::optional<std::string> result = AppendSignatureToHeaderAndPayload(
      "abc.efg", crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
      std::vector<uint8_t>({1, 2, 3}));
  EXPECT_EQ(result, std::nullopt);
}

}  // namespace net::device_bound_sessions
