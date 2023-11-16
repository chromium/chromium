// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/constants.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_prefs.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Checks that the visibility of the Safety Check module matches `should_show`.
void WaitUntilSafetyCheckModuleVisibleOrTimeout(bool should_show) {
  GREYCondition* module_shown = [GREYCondition
      conditionWithName:@"Module shown"
                  block:^BOOL {
                    NSError* error;
                    [[EarlGrey selectElementWithMatcher:
                                   grey_accessibilityID(
                                       safety_check::kSafetyCheckViewID)]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];

  // Wait for the module to be shown or timeout after
  // `kWaitForUIElementTimeout`.
  BOOL success = [module_shown
      waitWithTimeout:base::test::ios::kWaitForUIElementTimeout.InSecondsF()];

  if (should_show) {
    GREYAssertTrue(success, @"Module did not appear.");
  } else {
    GREYAssertFalse(success, @"Module appeared.");
  }
}

}  // namespace

// Test case for the Safety Check view, i.e. Safety Check (Magic Stack) module.
@interface SafetyCheckViewCase : ChromeTestCase
@end

@implementation SafetyCheckViewCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  config.additional_args.push_back(
      "--enable-features=" + std::string(kMagicStack.name) + "," +
      std::string(kSafetyCheckMagicStack.name));

  return config;
}

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey resetDataForLocalStatePref:
                      safety_check_prefs::kSafetyCheckInMagicStackDisabledPref];
}

- (void)tearDown {
  [ChromeEarlGrey resetDataForLocalStatePref:
                      safety_check_prefs::kSafetyCheckInMagicStackDisabledPref];

  [super tearDown];
}

// Tests that long pressing the Safety Check view displays a context menu; tests
// the Safety Check view is properly hidden via the context menu.
- (void)testLongPressAndHide {
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              safety_check::kSafetyCheckViewID),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionRight, 350)
      onElementWithMatcher:grey_accessibilityID(
                               kMagicStackScrollViewAccessibilityIdentifier)]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SAFETY_CHECK_CONTEXT_MENU_DESCRIPTION))]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SAFETY_CHECK_CONTEXT_MENU_DESCRIPTION))]
      performAction:grey_tap()];

  // Check that the module is hidden.
  WaitUntilSafetyCheckModuleVisibleOrTimeout(false);
}

// Tests that the Password Checkup view is dismissed when there are no saved
// passwords.
- (void)testPasswordCheckupDismissedAfterAllPasswordsGone {
  password_manager_test_utils::SavePasswordForm();

  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              safety_check::kSafetyCheckViewID),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionRight, 350)
      onElementWithMatcher:grey_accessibilityID(
                               kMagicStackScrollViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                     IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON))]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };

  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(10),
                                                          condition),
             @"Timeout waiting for the Safety Check to complete its run.");

  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SETTINGS_SAFETY_CHECK_PASSWORDS_TITLE))]
      performAction:grey_tap()];

  // Verify that the Password Checkup Homepage is displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(password_manager::kPasswordCheckupTableViewId)]
      assertWithMatcher:grey_notNil()];

  [PasswordSettingsAppInterface clearPasswordStore];

  // Verify that the Password Checkup Homepage is not displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(password_manager::kPasswordCheckupTableViewId)]
      assertWithMatcher:grey_nil()];
}

@end
