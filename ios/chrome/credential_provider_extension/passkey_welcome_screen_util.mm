// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_welcome_screen_util.h"

#import "base/check.h"
#import "components/webauthn/ios/passkey_types.h"
#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_strings.h"
#import "ios/chrome/credential_provider_extension/generated_localized_strings.h"

namespace {

using ::webauthn::PasskeyWelcomeScreenPurpose;

// Returns the title to use depending on the provided `purpose`.
NSString* GetTitleString(PasskeyWelcomeScreenPurpose purpose) {
  switch (purpose) {
    case PasskeyWelcomeScreenPurpose::kEnroll:
      return CredentialProviderPasskeyEnrollmentTitleString();
    case PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability:
      return CredentialProviderPasskeyPartialBootsrappingTitleString();
    case PasskeyWelcomeScreenPurpose::kReauthenticate:
      return CredentialProviderPasskeyBootsrappingTitleString();
  }
  return @"";
}

// Returns the subtitle to use depending on the provided `purpose`.
NSString* GetSubtitleString(PasskeyWelcomeScreenPurpose purpose) {
  switch (purpose) {
    case PasskeyWelcomeScreenPurpose::kEnroll:
      return nil;
    case PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability:
      return CredentialProviderPasskeyPartialBootsrappingSubtitleString();
    case PasskeyWelcomeScreenPurpose::kReauthenticate:
      return CredentialProviderPasskeyBootsrappingSubtitleString();
  }
  return @"";
}

// Returns the title to use for the primary button depending on the provided
// `purpose`.
NSString* GetPrimaryButtonTitle(PasskeyWelcomeScreenPurpose purpose) {
  switch (purpose) {
    case PasskeyWelcomeScreenPurpose::kEnroll:
    case PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability:
      return CredentialProviderGetStartedButtonString();
    case PasskeyWelcomeScreenPurpose::kReauthenticate:
      return CredentialProviderNextButtonString();
  }
  return @"";
}

// Returns an array of instructions or nil depending on the provided `purpose`.
NSArray<NSString*>* GetInstructions(PasskeyWelcomeScreenPurpose purpose) {
  if (purpose != PasskeyWelcomeScreenPurpose::kEnroll) {
    return nil;
  }

  return @[
    CredentialProviderPasskeyEnrollmentInstructionsStep1String(),
    CredentialProviderPasskeyEnrollmentInstructionsStep2String(),
    CredentialProviderPasskeyEnrollmentInstructionsStep3String(),
  ];
}

}  // namespace

PasskeyWelcomeScreenStrings* GetPasskeyWelcomeScreenStrings(
    PasskeyWelcomeScreenPurpose purpose,
    NSString* userEmail) {
  NSString* footer = nil;
  if (purpose == PasskeyWelcomeScreenPurpose::kEnroll) {
    CHECK(userEmail);
    NSString* stringWithPlaceholder =
        CredentialProviderPasskeyEnrollmentFooterMessageString();
    footer =
        [stringWithPlaceholder stringByReplacingOccurrencesOfString:@"$1"
                                                         withString:userEmail];
  }
  NSString* secondaryButton = CredentialProviderNotNowButtonString();

  return [[PasskeyWelcomeScreenStrings alloc]
        initWithTitle:GetTitleString(purpose)
             subtitle:GetSubtitleString(purpose)
               footer:footer
        primaryButton:GetPrimaryButtonTitle(purpose)
      secondaryButton:secondaryButton
         instructions:GetInstructions(purpose)];
}
