// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/generate_key_param.h"

#include <optional>

#include "base/test/gmock_expected_support.h"
#include "crypto/sign.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::device_bound_sessions {
namespace {

using ::testing::ElementsAre;

TEST(GenerateKeyParamTest, ParseValid) {
  ASSERT_OK_AND_ASSIGN(
      auto param,
      GenerateKeyParam::Parse(
          R"((ES256 RS256); target_origin="https://rp.com"; provider_session_id="session123"; challenge="chal456")"));
  EXPECT_THAT(
      param.supported_algos,
      ElementsAre(crypto::sign::ECDSA_SHA256, crypto::sign::RSA_PKCS1_SHA256));
  EXPECT_EQ(param.target_origin.Serialize(), "https://rp.com");
  EXPECT_EQ(param.provider_session_id.value(), "session123");
  EXPECT_EQ(param.challenge, "chal456");
}

TEST(GenerateKeyParamTest, ParseUnknownAlgorithmsIgnored) {
  ASSERT_OK_AND_ASSIGN(
      auto param,
      GenerateKeyParam::Parse(
          R"((ES256 UNKN1 RS256 TEST); target_origin="https://rp.com"; provider_session_id="session123"; challenge="chal456")"));
  EXPECT_THAT(
      param.supported_algos,
      ElementsAre(crypto::sign::ECDSA_SHA256, crypto::sign::RSA_PKCS1_SHA256));
}

TEST(GenerateKeyParamTest, ParseNoSupportedAlgorithms) {
  EXPECT_EQ(
      GenerateKeyParam::Parse(
          R"((UNKN1 TEST); target_origin="https://rp.com"; provider_session_id="session123"; challenge="chal456")"),
      std::nullopt);
}

TEST(GenerateKeyParamTest, ParseNonTokenAlgorithmsIgnored) {
  ASSERT_OK_AND_ASSIGN(
      auto param,
      GenerateKeyParam::Parse(
          R"(("ES256" RS256); target_origin="https://rp.com"; provider_session_id="session123"; challenge="chal456")"));
  EXPECT_THAT(param.supported_algos,
              ElementsAre(crypto::sign::RSA_PKCS1_SHA256));
}

TEST(GenerateKeyParamTest, ParseEmptyInnerList) {
  EXPECT_EQ(
      GenerateKeyParam::Parse(
          R"((); target_origin="https://rp.com"; provider_session_id="session123"; challenge="chal456")"),
      std::nullopt);
}

TEST(GenerateKeyParamTest, ParseEmptyHeaderValue) {
  EXPECT_EQ(GenerateKeyParam::Parse(""), std::nullopt);
}

TEST(GenerateKeyParamTest, ParseExtraParametersIgnored) {
  ASSERT_OK_AND_ASSIGN(
      auto param,
      GenerateKeyParam::Parse(
          R"((ES256); target_origin="https://rp.com"; provider_session_id="session123"; challenge="chal456"; extra="value")"));
  EXPECT_THAT(param.supported_algos, ElementsAre(crypto::sign::ECDSA_SHA256));
}

TEST(GenerateKeyParamTest, ParseDuplicateKeyLastWins) {
  ASSERT_OK_AND_ASSIGN(
      auto param,
      GenerateKeyParam::Parse(
          R"((ES256); target_origin="https://rp.com"; provider_session_id="session123"; challenge="first"; challenge="second")"));
  EXPECT_EQ(param.challenge, "second");
}

TEST(GenerateKeyParamTest, ParseMissingTargetOrigin) {
  EXPECT_EQ(
      GenerateKeyParam::Parse(
          R"((ES256); provider_session_id="session123"; challenge="chal456")"),
      std::nullopt);
}

TEST(GenerateKeyParamTest, ParseMissingProviderSessionId) {
  EXPECT_EQ(
      GenerateKeyParam::Parse(
          R"((ES256); target_origin="https://rp.com"; challenge="chal456")"),
      std::nullopt);
}

TEST(GenerateKeyParamTest, ParseMissingChallenge) {
  EXPECT_EQ(
      GenerateKeyParam::Parse(
          R"((ES256); target_origin="https://rp.com"; provider_session_id="session123")"),
      std::nullopt);
}

TEST(GenerateKeyParamTest, ParseEmptyProviderSessionId) {
  EXPECT_EQ(
      GenerateKeyParam::Parse(
          R"((ES256); target_origin="https://rp.com"; provider_session_id=""; challenge="chal456")"),
      std::nullopt);
}

TEST(GenerateKeyParamTest, ParseEmptyChallenge) {
  EXPECT_EQ(
      GenerateKeyParam::Parse(
          R"((ES256); target_origin="https://rp.com"; provider_session_id="session123"; challenge="")"),
      std::nullopt);
}

TEST(GenerateKeyParamTest, ParseWrongParameterType) {
  EXPECT_EQ(
      GenerateKeyParam::Parse(
          R"((ES256); target_origin="https://rp.com"; provider_session_id="session123"; challenge=?1)"),
      std::nullopt);
}

TEST(GenerateKeyParamTest, ParseNonStringParameters) {
  // `https://rp.com` is a valid token, but parameters must be strings.
  EXPECT_EQ(
      GenerateKeyParam::Parse(
          R"((ES256); target_origin=https://rp.com; provider_session_id="session123"; challenge="chal456")"),
      std::nullopt);
}

TEST(GenerateKeyParamTest, ParseInvalidOrigin) {
  EXPECT_EQ(
      GenerateKeyParam::Parse(
          R"((ES256); target_origin="not_a_url"; provider_session_id="session123"; challenge="chal456")"),
      std::nullopt);
}

TEST(GenerateKeyParamTest, ParseInsecureOrigin) {
  EXPECT_EQ(
      GenerateKeyParam::Parse(
          R"((ES256); target_origin="http://rp.com"; provider_session_id="session123"; challenge="chal456")"),
      std::nullopt);
}

TEST(GenerateKeyParamTest, ParseFileOrigin) {
  EXPECT_EQ(
      GenerateKeyParam::Parse(
          R"((ES256); target_origin="file:///"; provider_session_id="session123"; challenge="chal456")"),
      std::nullopt);
}

TEST(GenerateKeyParamTest, ParseNotInnerList) {
  EXPECT_EQ(
      GenerateKeyParam::Parse(
          R"(ES256; target_origin="https://rp.com"; provider_session_id="session123"; challenge="chal456")"),
      std::nullopt);
}

TEST(GenerateKeyParamTest, ParseUnterminatedInnerList) {
  EXPECT_EQ(
      GenerateKeyParam::Parse(
          R"((ES256 RS256; target_origin="https://rp.com"; provider_session_id="session123"; challenge="chal456")"),
      std::nullopt);
}

TEST(GenerateKeyParamTest, ParseMultipleInnerLists) {
  EXPECT_EQ(
      GenerateKeyParam::Parse(
          R"((ES256), (RS256); target_origin="https://rp.com"; provider_session_id="session123"; challenge="chal456")"),
      std::nullopt);
}

}  // namespace
}  // namespace net::device_bound_sessions
