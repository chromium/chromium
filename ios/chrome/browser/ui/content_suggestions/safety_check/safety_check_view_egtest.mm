// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ntp/home/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/constants.h"
#import "ios/chrome/grit/ios_strings.h"
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

@end
