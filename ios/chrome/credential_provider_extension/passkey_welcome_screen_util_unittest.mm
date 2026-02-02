// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_welcome_screen_util.h"

#import "components/webauthn/ios/passkey_types.h"
#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_strings.h"
#import "ios/chrome/credential_provider_extension/generated_localized_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

class PasskeyWelcomeScreenUtilTest : public PlatformTest {};

TEST_F(PasskeyWelcomeScreenUtilTest, TestStringsForEnrollmentPurpose) {
  NSString* email = @"user@example.com";
  PasskeyWelcomeScreenStrings* strings = GetPasskeyWelcomeScreenStrings(
      webauthn::PasskeyWelcomeScreenPurpose::kEnroll, email);

  EXPECT_NSEQ(strings.title, CredentialProviderPasskeyEnrollmentTitleString());
  EXPECT_FALSE(strings.subtitle);
  EXPECT_NSEQ(strings.footer,
              [CredentialProviderPasskeyEnrollmentFooterMessageString()
                  stringByReplacingOccurrencesOfString:@"$1"
                                            withString:email]);
  EXPECT_NSEQ(strings.primaryButton,
              CredentialProviderGetStartedButtonString());
  EXPECT_NSEQ(strings.secondaryButton, CredentialProviderNotNowButtonString());
  NSArray<NSString*>* expectedInstructions = @[
    CredentialProviderPasskeyEnrollmentInstructionsStep1String(),
    CredentialProviderPasskeyEnrollmentInstructionsStep2String(),
    CredentialProviderPasskeyEnrollmentInstructionsStep3String(),
  ];
  EXPECT_NSEQ(strings.instructions, expectedInstructions);
}

TEST_F(PasskeyWelcomeScreenUtilTest,
       TestStringsForFixDegradedRecoverabilityPurpose) {
  PasskeyWelcomeScreenStrings* strings = GetPasskeyWelcomeScreenStrings(
      webauthn::PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability,
      /*userEmail=*/nil);

  EXPECT_NSEQ(strings.title,
              CredentialProviderPasskeyPartialBootsrappingTitleString());
  EXPECT_NSEQ(strings.subtitle,
              CredentialProviderPasskeyPartialBootsrappingSubtitleString());
  EXPECT_FALSE(strings.footer);
  EXPECT_NSEQ(strings.primaryButton,
              CredentialProviderGetStartedButtonString());
  EXPECT_NSEQ(strings.secondaryButton, CredentialProviderNotNowButtonString());
  EXPECT_FALSE(strings.instructions);
}

TEST_F(PasskeyWelcomeScreenUtilTest, TestStringsForReauthenticationPurpose) {
  PasskeyWelcomeScreenStrings* strings = GetPasskeyWelcomeScreenStrings(
      webauthn::PasskeyWelcomeScreenPurpose::kReauthenticate,
      /*userEmail=*/nil);

  EXPECT_NSEQ(strings.title,
              CredentialProviderPasskeyBootsrappingTitleString());
  EXPECT_NSEQ(strings.subtitle,
              CredentialProviderPasskeyBootsrappingSubtitleString());
  EXPECT_FALSE(strings.footer);
  EXPECT_NSEQ(strings.primaryButton, CredentialProviderNextButtonString());
  EXPECT_NSEQ(strings.secondaryButton, CredentialProviderNotNowButtonString());
  EXPECT_FALSE(strings.instructions);
}

}  // namespace
