// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using password_manager_test_utils::SaveCompromisedPasswordFormToProfileStore;

namespace {

// Matcher for the compromised issues row in the Safety Check module.
// - issue_count: the expected number of issues displayed in the row.
id<GREYMatcher> CompromisedIssuesMatcher(int issue_count) {
  return grey_accessibilityLabel([NSString
      stringWithFormat:@"%@, %@", @"Passwords",
                       base::SysUTF16ToNSString(
                           l10n_util::GetPluralStringFUTF16(
                               IDS_IOS_PASSWORD_CHECKUP_COMPROMISED_COUNT,
                               issue_count))]);
}

// Matcher for the Password Checkup table view.
id<GREYMatcher> PasswordCheckupTableViewMatcher() {
  return grey_accessibilityID(password_manager::kPasswordCheckupTableViewId);
}

// Helper for verifying that the Password Checkup UI is visible.
void VerifyPasswordCheckupIsVisible() {
  [[EarlGrey selectElementWithMatcher:PasswordCheckupTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Helper for verifying that the Password Checkup UI is not visible.
void VerifyPasswordCheckupIsNotVisible() {
  [[EarlGrey selectElementWithMatcher:PasswordCheckupTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Helper for verifying that the Safety Check UI is visible.
void VerifySafetyCheckIsVisible() {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SafetyCheckTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Opens the Safety Check Module.
void OpenSafetyCheck() {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsMenuSafetyCheckButton()];

  VerifySafetyCheckIsVisible();
}

// Helper for tapping the Check Now button in the Safety Check module.
void TapCheckNowButton() {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSafetyCheckCheckNowButtonAccessibilityID)]
      performAction:grey_tap()];
}

// Waits for the password check to be done and Taps the compromised issues row
// in the Safety Check module.
void TapCompromisedIssues(int issue_count) {
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:CompromisedIssuesMatcher(issue_count)
                                  timeout:base::Seconds(10)];

  [[EarlGrey selectElementWithMatcher:CompromisedIssuesMatcher(issue_count)]
      performAction:grey_tap()];
}

// Opens the Password Checkup UI from the Safety Check module.
// Requires only one saved compromised credential in the Password Store.
void OpenPasswordCheckup() {
  OpenSafetyCheck();

  TapCheckNowButton();

  TapCompromisedIssues(/*issue_count=*/1);
}

// Cleans up the last password check timestamp from User Defaults.
void ResetLastPasswordCheckTimestamp() {
  [ChromeEarlGrey removeUserDefaultsObjectForKey:kTimestampOfLastIssueFoundKey];
}

}  // namespace

// Test case for the Safety Check module.
@interface SafetyCheckTestCase : ChromeTestCase
@end

@implementation SafetyCheckTestCase

- (void)setUp {
  [super setUp];

  // Set the FakeBulkLeakCheckService to return the idle state.
  [PasswordSettingsAppInterface
      setFakeBulkLeakCheckBufferedState:
          password_manager::BulkLeakCheckServiceInterface::State::kIdle];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Mock local authentication for opening Password Checkup.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  ResetLastPasswordCheckTimestamp();
}

- (void)tearDown {
  [PasswordSettingsAppInterface removeMockReauthenticationModule];
  [super tearDown];

  [PasswordSettingsAppInterface
      setFakeBulkLeakCheckBufferedState:
          password_manager::BulkLeakCheckServiceInterface::State::kIdle];

  ResetLastPasswordCheckTimestamp();
}

// Validates that the Safety Check module can be opened from the Settings UI.
- (void)testOpenSafetyCheck {
  OpenSafetyCheck();
}

// Opens the Password Checkup UI from the Safety Check module.
- (void)testOpenPasswordCheckup {
  SaveCompromisedPasswordFormToProfileStore();

  OpenPasswordCheckup();

  VerifyPasswordCheckupIsVisible();
}

// Opens the Password Checkup UI from the Safety Check module and fails local
// authentication. Validates that the Password Checkup content is not revealed
// and it is dismissed after the failed authentication.
- (void)testOpenPasswordCheckupWithFailedAuth {
  SaveCompromisedPasswordFormToProfileStore();

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kFailure];
  [PasswordSettingsAppInterface mockReauthenticationModuleShouldSkipReAuth:NO];

  OpenPasswordCheckup();

  VerifyPasswordCheckupIsNotVisible();

  [PasswordSettingsAppInterface mockReauthenticationModuleReturnMockedResult];

  // Password checkup should be dismissed and Safety Check module should be
  // visible again.
  VerifyPasswordCheckupIsNotVisible();
  VerifySafetyCheckIsVisible();
}

@end
