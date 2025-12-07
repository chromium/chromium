// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/safety_check/safety_check_pref_names.h"
#import "components/segmentation_platform/public/features.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/new_tab_page_app_interface.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/safety_check/model/safety_check_prefs.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/safety_check/public/safety_check_constants.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
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
  id<GREYMatcher> matcher =
      should_show ? grey_sufficientlyVisible() : grey_notVisible();
  GREYCondition* module_shown = [GREYCondition
      conditionWithName:@"Module shown"
                  block:^BOOL {
                    NSError* error;
                    [[EarlGrey selectElementWithMatcher:
                                   grey_accessibilityID(
                                       safety_check::kSafetyCheckViewID)]
                        assertWithMatcher:matcher
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
    GREYAssertTrue(success, @"Module was visible.");
  }
}

// Scrolls to the Safety Check module.
void ScrollToSafetyCheckModule() {
  id<GREYMatcher> magicStackScrollView =
      grey_accessibilityID(kMagicStackScrollViewAccessibilityIdentifier);
  CGFloat moduleSwipeAmount = kMagicStackWideWidth * 0.6;
  id<GREYMatcher> safetyCheckMatcher =
      grey_allOf(grey_accessibilityID(safety_check::kSafetyCheckViewID),
                 grey_sufficientlyVisible(), nil);

  [[[EarlGrey selectElementWithMatcher:safetyCheckMatcher]
         usingSearchAction:GREYScrollInDirectionWithStartPoint(
                               kGREYDirectionRight, moduleSwipeAmount, 0.9, 0.5)
      onElementWithMatcher:magicStackScrollView]
      assertWithMatcher:grey_sufficientlyVisible()];
}

}  // namespace

// Test case for the Safety Check view, i.e. Safety Check (Magic Stack) module.
@interface SafetyCheckViewTestCase : ChromeTestCase
@end

@implementation SafetyCheckViewTestCase

- (void)setUp {
  [super setUp];

  [NewTabPageAppInterface disableTipsCards];
}

- (void)tearDownHelper {
  [[self class] closeAllTabs];

  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back("--test-ios-module-ranker=safety_check");

  return config;
}

// Tests that long pressing the Safety Check view displays a context menu; tests
// the Safety Check view is properly hidden via the context menu.
- (void)testLongPressAndHide {
  // Enable relevant preferences for the test.
  [ChromeEarlGrey
      setBoolValue:YES
       forUserPref:safety_check::prefs::kSafetyCheckHomeModuleEnabled];

  // Intentionally forces a Safety Check error to ensure module visibility in
  // the Magic Stack.
  [ChromeEarlGrey
         setStringValue:NameForSafetyCheckState(
                            SafeBrowsingSafetyCheckState::kUnsafe)
      forLocalStatePref:prefs::kIosSafetyCheckManagerSafeBrowsingCheckResult];

  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:[self appConfigurationForTestCase]];

  [ChromeEarlGrey openNewTab];

  // Scroll to the Safety Check module in case it is not the first module.
  ScrollToSafetyCheckModule();

  WaitUntilSafetyCheckModuleVisibleOrTimeout(true);

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              safety_check::kSafetyCheckViewID),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SAFETY_CHECK_CONTEXT_MENU_DESCRIPTION))]
      performAction:grey_tap()];

  // Check that the module is hidden.
  WaitUntilSafetyCheckModuleVisibleOrTimeout(false);
}

@end
