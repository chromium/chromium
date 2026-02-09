// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/public/passkey_welcome_screen_util.h"

#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_strings.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

using PasskeyWelcomeScreenUtilTest = PlatformTest;

TEST_F(PasskeyWelcomeScreenUtilTest, ReturnsCorrectStringsForEnroll) {
  std::string user_email = "test@example.com";
  PasskeyWelcomeScreenStrings* result = GetPasskeyWelcomeScreenStrings(
      webauthn::PasskeyWelcomeScreenPurpose::kEnroll, user_email);

  EXPECT_NSEQ(result.title,
              l10n_util::GetNSString(IDS_IOS_PASSKEY_ENROLLMENT_TITLE));
  EXPECT_FALSE(result.subtitle);
  EXPECT_NSEQ(result.footer,
              l10n_util::GetNSStringF(IDS_IOS_PASSKEY_ENROLLMENT_FOOTER_MESSAGE,
                                      base::UTF8ToUTF16(user_email)));
  EXPECT_NSEQ(result.primaryButton,
              l10n_util::GetNSString(
                  IDS_IOS_PASSKEY_WELCOME_SCREEN_GET_STARTED_BUTTON));
  EXPECT_NSEQ(
      result.secondaryButton,
      l10n_util::GetNSString(IDS_IOS_PASSKEY_ENROLLMENT_NOT_NOW_BUTTON));

  NSArray<NSString*>* expectedInstructions = @[
    l10n_util::GetNSString(IDS_IOS_PASSKEY_ENROLLMENT_INSTRUCTIONS_STEP_1),
    l10n_util::GetNSString(IDS_IOS_PASSKEY_ENROLLMENT_INSTRUCTIONS_STEP_2),
    l10n_util::GetNSString(IDS_IOS_PASSKEY_ENROLLMENT_INSTRUCTIONS_STEP_3),
  ];
  EXPECT_NSEQ(result.instructions, expectedInstructions);
}

TEST_F(PasskeyWelcomeScreenUtilTest,
       ReturnsCorrectStringsForFixDegradedRecoverability) {
  PasskeyWelcomeScreenStrings* result = GetPasskeyWelcomeScreenStrings(
      webauthn::PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability,
      /*user_email=*/"");

  EXPECT_NSEQ(result.title, l10n_util::GetNSString(
                                IDS_IOS_PASSKEY_PARTIAL_BOOTSTRAPPING_TITLE));
  EXPECT_NSEQ(
      result.subtitle,
      l10n_util::GetNSString(IDS_IOS_PASSKEY_PARTIAL_BOOTSTRAPPING_SUBTITLE));
  EXPECT_FALSE(result.footer);
  EXPECT_NSEQ(result.primaryButton,
              l10n_util::GetNSString(
                  IDS_IOS_PASSKEY_WELCOME_SCREEN_GET_STARTED_BUTTON));
  EXPECT_NSEQ(
      result.secondaryButton,
      l10n_util::GetNSString(IDS_IOS_PASSKEY_ENROLLMENT_NOT_NOW_BUTTON));
  EXPECT_FALSE(result.instructions);
}

TEST_F(PasskeyWelcomeScreenUtilTest, ReturnsCorrectStringsForReauthenticate) {
  PasskeyWelcomeScreenStrings* result = GetPasskeyWelcomeScreenStrings(
      webauthn::PasskeyWelcomeScreenPurpose::kReauthenticate,
      /*user_email=*/"");

  EXPECT_NSEQ(result.title,
              l10n_util::GetNSString(IDS_IOS_PASSKEY_BOOTSTRAPPING_TITLE));
  EXPECT_NSEQ(result.subtitle,
              l10n_util::GetNSString(IDS_IOS_PASSKEY_BOOTSTRAPPING_SUBTITLE));
  EXPECT_FALSE(result.footer);
  EXPECT_NSEQ(
      result.primaryButton,
      l10n_util::GetNSString(IDS_IOS_PASSKEY_WELCOME_SCREEN_NEXT_BUTTON));
  EXPECT_NSEQ(
      result.secondaryButton,
      l10n_util::GetNSString(IDS_IOS_PASSKEY_ENROLLMENT_NOT_NOW_BUTTON));
  EXPECT_FALSE(result.instructions);
}

}  // namespace
