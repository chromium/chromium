// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/time/time.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
base::TimeDelta kPromoAppearanceTimeout = base::Seconds(7);

// Matcher for the title.
id<GREYMatcher> TitleMatcher() {
  return grey_text(
      l10n_util::GetNSString(IDS_IOS_POST_RESTORE_DEFAULT_BROWSER_PROMO_TITLE));
}

// Matcher for the "Open Settings" button.
id<GREYMatcher> PrimaryActionMatcher() {
  return grey_text(l10n_util::GetNSString(
      IDS_IOS_POST_RESTORE_DEFAULT_BROWSER_PRIMARY_ACTION));
}

// Matcher for the "No Thanks" button.
id<GREYMatcher> SecondaryActionMatcher() {
  return grey_text(l10n_util::GetNSString(
      IDS_IOS_POST_RESTORE_DEFAULT_BROWSER_SECONDARY_ACTION));
}
}  // namespace

// Tests related to the Post Restore Default Browser Promo.
@interface PostRestoreDefaultBrowserPromoTestCase : ChromeTestCase
@end

@implementation PostRestoreDefaultBrowserPromoTestCase

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.additional_args.push_back("-enable-promo-manager-fullscreen-promos");
  // Override trigger requirements to force the promo to appear.
  config.additional_args.push_back("-NextPromoForDisplayOverride");
  config.additional_args.push_back(
      "promos_manager::Promo::PostRestoreDefaultBrowserAlert");
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

#pragma mark - Helpers

- (void)checkThatCommonElementsAreVisible {
  [[EarlGrey selectElementWithMatcher:TitleMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:PrimaryActionMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:SecondaryActionMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)checkThatCommonElementsAreNotVisible {
  [[EarlGrey selectElementWithMatcher:TitleMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:PrimaryActionMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:SecondaryActionMatcher()]
      assertWithMatcher:grey_notVisible()];
}

#pragma mark - Tests

// Verifies that the secondary action button opens dismisses the promo.
// TODO(crbug.com/40929054): re-enable once test is no longer flaky.
- (void)DISABLED_testDismiss {
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TitleMatcher()
                                              timeout:kPromoAppearanceTimeout];
  [self checkThatCommonElementsAreVisible];
  [[EarlGrey selectElementWithMatcher:SecondaryActionMatcher()]
      performAction:grey_tap()];
  [self checkThatCommonElementsAreNotVisible];
}

@end
