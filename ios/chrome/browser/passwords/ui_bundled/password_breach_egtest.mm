// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_breach_app_interface.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

using chrome_test_util::ButtonWithAccessibilityLabel;

id<GREYMatcher> PasswordBreachMatcher() {
  return grey_accessibilityID(kPasswordBreachViewAccessibilityIdentifier);
}

id<GREYMatcher> PasswordListMatcher() {
  return grey_accessibilityID(kPasswordsTableViewID);
}

id<GREYMatcher> CheckPasswordButton() {
  return grey_allOf(ButtonWithAccessibilityLabel(base::SysUTF16ToNSString(
                        l10n_util::GetStringUTF16(IDS_LEAK_CHECK_CREDENTIALS))),
                    grey_interactable(), nullptr);
}

}  // namespace

@interface PasswordBreachTestCase : ChromeTestCase
@end

@implementation PasswordBreachTestCase

#pragma mark - Tests

- (void)testPasswordBreachIsPresented {
  [PasswordBreachAppInterface showPasswordBreachWithCheckButton:NO];
  [[EarlGrey selectElementWithMatcher:PasswordBreachMatcher()]
      assertWithMatcher:grey_notNil()];
}

// Tests that Check password button redirects to the Passwords List.
- (void)testPasswordBreachRedirectToPasswords {
  [PasswordBreachAppInterface showPasswordBreachWithCheckButton:YES];
  [[EarlGrey selectElementWithMatcher:PasswordBreachMatcher()]
      assertWithMatcher:grey_notNil()];

  // Mock successful auth for opening the password manager.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [[EarlGrey selectElementWithMatcher:CheckPasswordButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordListMatcher()]
      assertWithMatcher:grey_notNil()];

  // Cleanup mock reauth module.
  [PasswordSettingsAppInterface removeMockReauthenticationModule];
}

@end
