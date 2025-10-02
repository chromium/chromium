// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_welcome_screen_util.h"

#import "base/check.h"
#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_strings.h"

namespace {

// Returns the title to use depending on the provided `purpose`.
NSString* GetTitleString(PasskeyWelcomeScreenPurpose purpose) {
  NSString* stringID;
  switch (purpose) {
    case PasskeyWelcomeScreenPurpose::kEnroll:
      stringID = @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_ENROLLMENT_TITLE";
      break;
    case PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability:
      stringID =
          @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_PARTIAL_BOOTSRAPPING_TITLE";
      break;
    case PasskeyWelcomeScreenPurpose::kReauthenticate:
      stringID = @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_BOOTSRAPPING_TITLE";
      break;
  }
  return NSLocalizedString(stringID, @"The title of the welcome screen.");
}

// Returns the subtitle to use depending on the provided `purpose`.
NSString* GetSubtitleString(PasskeyWelcomeScreenPurpose purpose) {
  NSString* stringID;
  switch (purpose) {
    case PasskeyWelcomeScreenPurpose::kEnroll:
      return nil;
    case PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability:
      stringID =
          @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_PARTIAL_BOOTSRAPPING_SUBTITLE";
      break;
    case PasskeyWelcomeScreenPurpose::kReauthenticate:
      stringID = @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_BOOTSRAPPING_SUBTITLE";
      break;
  }
  return NSLocalizedString(stringID, @"The subtitle of the welcome screen.");
}

// Returns the title to use for the primary button depending on the provided
// `purpose`.
NSString* GetPrimaryButtonTitle(PasskeyWelcomeScreenPurpose purpose) {
  NSString* stringID;
  switch (purpose) {
    case PasskeyWelcomeScreenPurpose::kEnroll:
    case PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability:
      stringID = @"IDS_IOS_CREDENTIAL_PROVIDER_GET_STARTED_BUTTON";
      break;
    case PasskeyWelcomeScreenPurpose::kReauthenticate:
      stringID = @"IDS_IOS_CREDENTIAL_PROVIDER_NEXT_BUTTON";
      break;
  }
  return NSLocalizedString(
      stringID, @"The title of the welcome screen's primary button.");
}

// Returns an array of instructions or nil depending on the provided `purpose`.
NSArray<NSString*>* GetInstructions(PasskeyWelcomeScreenPurpose purpose) {
  if (purpose != PasskeyWelcomeScreenPurpose::kEnroll) {
    return nil;
  }

  return @[
    NSLocalizedString(
        @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_ENROLLMENT_INSTRUCTIONS_STEP_1",
        @"First step of the passkey enrollment instructions"),
    NSLocalizedString(
        @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_ENROLLMENT_INSTRUCTIONS_STEP_2",
        @"Second step of the passkey enrollment instructions"),
    NSLocalizedString(
        @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_ENROLLMENT_INSTRUCTIONS_STEP_3",
        @"Third step of the passkey enrollment instructions"),
  ];
}

}  // namespace

PasskeyWelcomeScreenStrings* GetPasskeyWelcomeScreenStrings(
    PasskeyWelcomeScreenPurpose purpose,
    NSString* userEmail) {
  NSString* footer = nil;
  if (purpose == PasskeyWelcomeScreenPurpose::kEnroll) {
    CHECK(userEmail);
    NSString* stringWithPlaceholder = NSLocalizedString(
        @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_ENROLLMENT_FOOTER_MESSAGE",
        @"Footer message shown at the bottom of the screen-specific view.");
    footer =
        [stringWithPlaceholder stringByReplacingOccurrencesOfString:@"$1"
                                                         withString:userEmail];
  }
  NSString* secondaryButton =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_NOT_NOW_BUTTON",
                        @"The title of the welcome screen's secondary button.");

  return [[PasskeyWelcomeScreenStrings alloc]
        initWithTitle:GetTitleString(purpose)
             subtitle:GetSubtitleString(purpose)
               footer:footer
        primaryButton:GetPrimaryButtonTitle(purpose)
      secondaryButton:secondaryButton
         instructions:GetInstructions(purpose)];
}
