// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/public/passkey_welcome_screen_util.h"

#import "base/check.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_strings.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

using ::webauthn::PasskeyWelcomeScreenPurpose;

// Returns the title to use depending on the provided `purpose`.
NSString* GetTitleString(PasskeyWelcomeScreenPurpose purpose) {
  switch (purpose) {
    case PasskeyWelcomeScreenPurpose::kEnroll:
      return l10n_util::GetNSString(IDS_IOS_PASSKEY_ENROLLMENT_TITLE);
    case PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability:
      return l10n_util::GetNSString(
          IDS_IOS_PASSKEY_PARTIAL_BOOTSTRAPPING_TITLE);
    case PasskeyWelcomeScreenPurpose::kReauthenticate:
      return l10n_util::GetNSString(IDS_IOS_PASSKEY_BOOTSTRAPPING_TITLE);
  }
}

// Returns the subtitle to use depending on the provided `purpose`.
NSString* GetSubtitleString(PasskeyWelcomeScreenPurpose purpose) {
  switch (purpose) {
    case PasskeyWelcomeScreenPurpose::kEnroll:
      return nil;
    case PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability:
      return l10n_util::GetNSString(
          IDS_IOS_PASSKEY_PARTIAL_BOOTSTRAPPING_SUBTITLE);
    case PasskeyWelcomeScreenPurpose::kReauthenticate:
      return l10n_util::GetNSString(IDS_IOS_PASSKEY_BOOTSTRAPPING_SUBTITLE);
  }
}

// Returns the title to use for the primary button depending on the provided
// `purpose`.
NSString* GetPrimaryButtonTitle(PasskeyWelcomeScreenPurpose purpose) {
  switch (purpose) {
    case PasskeyWelcomeScreenPurpose::kEnroll:
    case PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability:
      return l10n_util::GetNSString(
          IDS_IOS_PASSKEY_WELCOME_SCREEN_GET_STARTED_BUTTON);
    case PasskeyWelcomeScreenPurpose::kReauthenticate:
      return l10n_util::GetNSString(IDS_IOS_PASSKEY_WELCOME_SCREEN_NEXT_BUTTON);
  }
}

// Returns an array of instructions or nil depending on the provided `purpose`.
NSArray<NSString*>* GetInstructions(PasskeyWelcomeScreenPurpose purpose) {
  if (purpose != PasskeyWelcomeScreenPurpose::kEnroll) {
    return nil;
  }
  return @[
    l10n_util::GetNSString(IDS_IOS_PASSKEY_ENROLLMENT_INSTRUCTIONS_STEP_1),
    l10n_util::GetNSString(IDS_IOS_PASSKEY_ENROLLMENT_INSTRUCTIONS_STEP_2),
    l10n_util::GetNSString(IDS_IOS_PASSKEY_ENROLLMENT_INSTRUCTIONS_STEP_3),
  ];
}

}  // namespace

PasskeyWelcomeScreenStrings* GetPasskeyWelcomeScreenStrings(
    PasskeyWelcomeScreenPurpose purpose,
    std::string userEmail) {
  NSString* footer = nil;
  if (purpose == PasskeyWelcomeScreenPurpose::kEnroll) {
    footer = l10n_util::GetNSStringF(IDS_IOS_PASSKEY_ENROLLMENT_FOOTER_MESSAGE,
                                     base::UTF8ToUTF16(std::move(userEmail)));
  }
  NSString* secondaryButton =
      l10n_util::GetNSString(IDS_IOS_PASSKEY_ENROLLMENT_NOT_NOW_BUTTON);

  return [[PasskeyWelcomeScreenStrings alloc]
        initWithTitle:GetTitleString(purpose)
             subtitle:GetSubtitleString(purpose)
               footer:footer
        primaryButton:GetPrimaryButtonTitle(purpose)
      secondaryButton:secondaryButton
         instructions:GetInstructions(purpose)];
}
