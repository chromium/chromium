// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_breach_app_interface.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_checkup/password_checkup_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings_app_interface.h"
#import "ios/chrome/browser/settings/ui_bundled/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
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

id<GREYMatcher> PasswordCheckupHomepageMatcher() {
  return grey_accessibilityID(password_manager::kPasswordCheckupTableViewId);
}

id<GREYMatcher> PasswordManagerMatcher() {
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

- (void)setUp {
  [super setUp];

  // Set up histogram tester.
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
}

- (void)tearDownHelper {
  // Clean up histogram tester.
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface releaseHistogramTester]);

  [PasswordSettingsAppInterface removeMockReauthenticationModule];
  [SigninEarlGrey signOut];
  [super tearDownHelper];
}

#pragma mark - Tests

- (void)testPasswordBreachIsPresented {
  [PasswordBreachAppInterface showPasswordBreachWithCheckButton:NO];
  [[EarlGrey selectElementWithMatcher:PasswordBreachMatcher()]
      assertWithMatcher:grey_notNil()];
}

// Tests that the "Check passwords" button redirects to the Password Checkup
// homepage when the user is signed in.
- (void)testPasswordBreachRedirectsToPasswordCheckup {
  // Sign in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  [PasswordBreachAppInterface showPasswordBreachWithCheckButton:YES];
  [[EarlGrey selectElementWithMatcher:PasswordBreachMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Mock successful auth for opening the Password Checkup homepage.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [[EarlGrey selectElementWithMatcher:CheckPasswordButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordCheckupHomepageMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:
                                static_cast<int>(
                                    password_manager::PasswordCheckReferrer::
                                        kPasswordBreachDialog)
                         forHistogram:@"PasswordManager.BulkCheck."
                                      @"PasswordCheckReferrer"],
      @"Erroneous logging of the navigation to the Password Checkup page.");
}

// Tests that the "Check passwords" button redirects to the Password Manager
// when the user is signed out.
- (void)testPasswordBreachRedirectsToPasswordManager {
  [PasswordBreachAppInterface showPasswordBreachWithCheckButton:YES];
  [[EarlGrey selectElementWithMatcher:PasswordBreachMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Mock successful auth for opening the Password Manager.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [[EarlGrey selectElementWithMatcher:CheckPasswordButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordManagerMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:
                                static_cast<int>(
                                    password_manager::ManagePasswordsReferrer::
                                        kPasswordBreachDialog)
                         forHistogram:
                             @"PasswordManager.ManagePasswordsReferrer"],
      @"Erroneous logging of the navigation to the Password Manager page.");
}

@end
