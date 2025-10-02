// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_welcome_screen_util.h"

#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

class PasskeyWelcomeScreenUtilTest : public PlatformTest {};

TEST_F(PasskeyWelcomeScreenUtilTest, TestStringsForEnrollmentPurpose) {
  NSString* email = @"user@example.com";
  PasskeyWelcomeScreenStrings* strings = GetPasskeyWelcomeScreenStrings(
      PasskeyWelcomeScreenPurpose::kEnroll, email);

  EXPECT_NSEQ(strings.title,
              @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_ENROLLMENT_TITLE");
  EXPECT_FALSE(strings.subtitle);
  EXPECT_NSEQ(strings.footer,
              [@"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_ENROLLMENT_FOOTER_MESSAGE"
                  stringByReplacingOccurrencesOfString:@"$1"
                                            withString:email]);
  EXPECT_NSEQ(strings.primaryButton,
              @"IDS_IOS_CREDENTIAL_PROVIDER_GET_STARTED_BUTTON");
  EXPECT_NSEQ(strings.secondaryButton,
              @"IDS_IOS_CREDENTIAL_PROVIDER_NOT_NOW_BUTTON");
  NSArray<NSString*>* expectedInstructions = @[
    @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_ENROLLMENT_INSTRUCTIONS_STEP_1",
    @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_ENROLLMENT_INSTRUCTIONS_STEP_2",
    @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_ENROLLMENT_INSTRUCTIONS_STEP_3",
  ];
  EXPECT_NSEQ(strings.instructions, expectedInstructions);
}

TEST_F(PasskeyWelcomeScreenUtilTest,
       TestStringsForFixDegradedRecoverabilityPurpose) {
  PasskeyWelcomeScreenStrings* strings = GetPasskeyWelcomeScreenStrings(
      PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability,
      /*userEmail=*/nil);

  EXPECT_NSEQ(
      strings.title,
      @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_PARTIAL_BOOTSRAPPING_TITLE");
  EXPECT_NSEQ(
      strings.subtitle,
      @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_PARTIAL_BOOTSRAPPING_SUBTITLE");
  EXPECT_FALSE(strings.footer);
  EXPECT_NSEQ(strings.primaryButton,
              @"IDS_IOS_CREDENTIAL_PROVIDER_GET_STARTED_BUTTON");
  EXPECT_NSEQ(strings.secondaryButton,
              @"IDS_IOS_CREDENTIAL_PROVIDER_NOT_NOW_BUTTON");
  EXPECT_FALSE(strings.instructions);
}

TEST_F(PasskeyWelcomeScreenUtilTest, TestStringsForReauthenticationPurpose) {
  PasskeyWelcomeScreenStrings* strings = GetPasskeyWelcomeScreenStrings(
      PasskeyWelcomeScreenPurpose::kReauthenticate, /*userEmail=*/nil);

  EXPECT_NSEQ(strings.title,
              @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_BOOTSRAPPING_TITLE");
  EXPECT_NSEQ(strings.subtitle,
              @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_BOOTSRAPPING_SUBTITLE");
  EXPECT_FALSE(strings.footer);
  EXPECT_NSEQ(strings.primaryButton,
              @"IDS_IOS_CREDENTIAL_PROVIDER_NEXT_BUTTON");
  EXPECT_NSEQ(strings.secondaryButton,
              @"IDS_IOS_CREDENTIAL_PROVIDER_NOT_NOW_BUTTON");
  EXPECT_FALSE(strings.instructions);
}

}  // namespace
