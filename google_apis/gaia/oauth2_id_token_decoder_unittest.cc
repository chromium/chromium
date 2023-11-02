// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_id_token_decoder.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kIdTokenInvalidJwt[] =
    "dummy-header."
    "..."
    ".dummy-signature";
const char kIdTokenInvalidJson[] =
    "dummy-header."
    "YWJj"  // payload: abc
    ".dummy-signature";
const char kIdTokenEmptyServices[] =
    "dummy-header."
    "eyAic2VydmljZXMiOiBbXSB9"  // payload: { "services": [] }
    ".dummy-signature";
const char kIdTokenEmptyServicesHeaderSignature[] =
    "."
    "eyAic2VydmljZXMiOiBbXSB9"  // payload: { "services": [] }
    ".";
const char kIdTokenMissingServices[] =
    "dummy-header."
    "eyAiYWJjIjogIiJ9"  // payload: { "abc": ""}
    ".dummy-signature";
const char kIdTokenNotChildAccount[] =
    "dummy-header."
    "eyAic2VydmljZXMiOiBbImFiYyJdIH0="  // payload: { "services": ["abc"] }
    ".dummy-signature";
const char kIdTokenChildAccount[] =
    "dummy-header."
    "eyAic2VydmljZXMiOiBbInVjYSJdIH0="  // payload: { "services": ["uca"] }
    ".dummy-signature";
const char kIdTokenAdvancedProtectionAccount[] =
    "dummy-header."
    "eyAic2VydmljZXMiOiBbInRpYSJdIH0="  // payload: { "services": ["tia"] }
    ".dummy-signature";
const char kIdTokenChildAndAdvancedProtectionAccount[] =
    "dummy-header."
    "eyAic2VydmljZXMiOiBbInRpYSIsICJ1Y2EiXSB9"
    ".dummy-signature";  // payload: { "services": ["tia", "uca"] }

class OAuth2IdTokenDecoderTest : public testing::Test {};

TEST_F(OAuth2IdTokenDecoderTest, Invalid) {
  EXPECT_FALSE(gaia::ParseServiceFlags(kIdTokenInvalidJwt).is_child_account);

  EXPECT_FALSE(gaia::ParseServiceFlags(kIdTokenInvalidJson).is_child_account);

  EXPECT_FALSE(
      gaia::ParseServiceFlags(kIdTokenMissingServices).is_child_account);
}

TEST_F(OAuth2IdTokenDecoderTest, NotChild) {
  EXPECT_FALSE(gaia::ParseServiceFlags(kIdTokenEmptyServices).is_child_account);

  EXPECT_FALSE(gaia::ParseServiceFlags(kIdTokenEmptyServicesHeaderSignature)
                   .is_child_account);

  EXPECT_FALSE(
      gaia::ParseServiceFlags(kIdTokenNotChildAccount).is_child_account);
}

TEST_F(OAuth2IdTokenDecoderTest, Child) {
  EXPECT_TRUE(gaia::ParseServiceFlags(kIdTokenChildAccount).is_child_account);
}

TEST_F(OAuth2IdTokenDecoderTest, NotAdvancedProtection) {
  EXPECT_FALSE(gaia::ParseServiceFlags(kIdTokenEmptyServices)
                   .is_under_advanced_protection);

  EXPECT_FALSE(gaia::ParseServiceFlags(kIdTokenEmptyServicesHeaderSignature)
                   .is_under_advanced_protection);

  EXPECT_FALSE(gaia::ParseServiceFlags(kIdTokenChildAccount)
                   .is_under_advanced_protection);
}

TEST_F(OAuth2IdTokenDecoderTest, AdvancedProtection) {
  EXPECT_TRUE(gaia::ParseServiceFlags(kIdTokenAdvancedProtectionAccount)
                  .is_under_advanced_protection);

  gaia::TokenServiceFlags service_flags =
      gaia::ParseServiceFlags(kIdTokenChildAndAdvancedProtectionAccount);
  EXPECT_TRUE(service_flags.is_child_account);
  EXPECT_TRUE(service_flags.is_under_advanced_protection);
}

}  // namespace
