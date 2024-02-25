// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace signed_exchange_utils {

TEST(SignedExchangeUtilsTest, VersionParam_WrongEssence) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-foo");
  EXPECT_FALSE(version.has_value());
}

TEST(SignedExchangeUtilsTest, VersionParam_None) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange");
  EXPECT_FALSE(version.has_value());
}

TEST(SignedExchangeUtilsTest, VersionParam_NoneWithSemicolon) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;");
  EXPECT_FALSE(version.has_value());
}

TEST(SignedExchangeUtilsTest, VersionParam_Empty) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=");
  EXPECT_FALSE(version.has_value());
}

TEST(SignedExchangeUtilsTest, VersionParam_EmptyString) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\"\"");
  EXPECT_EQ(version, SignedExchangeVersion::kUnknown);
}

TEST(SignedExchangeUtilsTest, VersionParam_UnknownVersion) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=foobar");
  EXPECT_EQ(version, SignedExchangeVersion::kUnknown);
}

TEST(SignedExchangeUtilsTest, VersionParam_UnsupportedVersion) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=b2");
  EXPECT_EQ(version, SignedExchangeVersion::kUnknown);
}

TEST(SignedExchangeUtilsTest, VersionParam_Simple) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=b3");
  EXPECT_EQ(version, SignedExchangeVersion::kB3);
}

TEST(SignedExchangeUtilsTest, VersionParam_WithSpace) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange ; v=b3");
  EXPECT_EQ(version, SignedExchangeVersion::kB3);
}

TEST(SignedExchangeUtilsTest, VersionParam_ExtraParam) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=b3;foo=bar");
  EXPECT_EQ(version, SignedExchangeVersion::kB3);
}

TEST(SignedExchangeUtilsTest, VersionParam_Quoted) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\"b3\"");
  EXPECT_EQ(version, SignedExchangeVersion::kB3);
}

TEST(SignedExchangeUtilsTest, VersionParam_QuotedNonB3) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\"b32\"");
  EXPECT_EQ(version, SignedExchangeVersion::kUnknown);
}

TEST(SignedExchangeUtilsTest, VersionParam_QuotedLeadingWhitespace) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\" b3\"");
  EXPECT_EQ(version, SignedExchangeVersion::kUnknown);
}

TEST(SignedExchangeUtilsTest, VersionParam_QuotedTrailingWhitespace) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\"b3 \"");
  EXPECT_EQ(version, SignedExchangeVersion::kUnknown);
}

TEST(SignedExchangeUtilsTest, VersionParam_QuotesOpen) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\"b3");
  EXPECT_EQ(version, SignedExchangeVersion::kB3);
}

TEST(SignedExchangeUtilsTest, VersionParam_QuotesOpenNonV) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\"b3;r=\"b3");
  EXPECT_EQ(version, SignedExchangeVersion::kUnknown);
}

TEST(SignedExchangeUtilsTest, VersionParam_QuotesOpenNonV2) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\"b3\";r=\"b3");
  EXPECT_EQ(version, SignedExchangeVersion::kB3);
}

TEST(SignedExchangeUtilsTest, VersionParam_UseCaseInsensitiveMatch) {
  std::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("Application/Signed-Exchange;V=b3");
  EXPECT_EQ(version, SignedExchangeVersion::kB3);
}

}  // namespace signed_exchange_utils
}  // namespace content
