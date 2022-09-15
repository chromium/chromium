// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace signed_exchange_utils {

TEST(SignedExchangeUtilsTest, VersionParam_WrongEssence) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-foo");
  EXPECT_FALSE(version.has_value());
}

TEST(SignedExchangeUtilsTest, VersionParam_None) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange");
  EXPECT_FALSE(version.has_value());
}

TEST(SignedExchangeUtilsTest, VersionParam_NoneWithSemicolon) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;");
  EXPECT_FALSE(version.has_value());
}

TEST(SignedExchangeUtilsTest, VersionParam_Empty) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=");
  EXPECT_FALSE(version.has_value());
}

TEST(SignedExchangeUtilsTest, VersionParam_EmptyString) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\"\"");
  EXPECT_EQ(version, SignedExchangeVersion::kUnknown);
}

TEST(SignedExchangeUtilsTest, VersionParam_UnknownVersion) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=foobar");
  EXPECT_EQ(version, SignedExchangeVersion::kUnknown);
}

TEST(SignedExchangeUtilsTest, VersionParam_UnsupportedVersion) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=b2");
  EXPECT_EQ(version, SignedExchangeVersion::kUnknown);
}

TEST(SignedExchangeUtilsTest, VersionParam_Simple) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=b3");
  EXPECT_EQ(version, SignedExchangeVersion::kB3);
}

TEST(SignedExchangeUtilsTest, VersionParam_WithSpace) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange ; v=b3");
  EXPECT_EQ(version, SignedExchangeVersion::kB3);
}

TEST(SignedExchangeUtilsTest, VersionParam_ExtraParam) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=b3;foo=bar");
  EXPECT_EQ(version, SignedExchangeVersion::kB3);
}

TEST(SignedExchangeUtilsTest, VersionParam_Quoted) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\"b3\"");
  EXPECT_EQ(version, SignedExchangeVersion::kB3);
}

TEST(SignedExchangeUtilsTest, VersionParam_QuotedNonB3) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\"b32\"");
  EXPECT_EQ(version, SignedExchangeVersion::kUnknown);
}

TEST(SignedExchangeUtilsTest, VersionParam_QuotedLeadingWhitespace) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\" b3\"");
  EXPECT_EQ(version, SignedExchangeVersion::kUnknown);
}

TEST(SignedExchangeUtilsTest, VersionParam_QuotedTrailingWhitespace) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\"b3 \"");
  EXPECT_EQ(version, SignedExchangeVersion::kUnknown);
}

TEST(SignedExchangeUtilsTest, VersionParam_QuotesOpen) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\"b3");
  EXPECT_EQ(version, SignedExchangeVersion::kB3);
}

TEST(SignedExchangeUtilsTest, VersionParam_QuotesOpenNonV) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\"b3;r=\"b3");
  EXPECT_EQ(version, SignedExchangeVersion::kUnknown);
}

TEST(SignedExchangeUtilsTest, VersionParam_QuotesOpenNonV2) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("application/signed-exchange;v=\"b3\";r=\"b3");
  EXPECT_EQ(version, SignedExchangeVersion::kB3);
}

TEST(SignedExchangeUtilsTest, VersionParam_UseCaseInsensitiveMatch) {
  absl::optional<SignedExchangeVersion> version =
      GetSignedExchangeVersion("Application/Signed-Exchange;V=b3");
  EXPECT_EQ(version, SignedExchangeVersion::kB3);
}

}  // namespace signed_exchange_utils
}  // namespace content
