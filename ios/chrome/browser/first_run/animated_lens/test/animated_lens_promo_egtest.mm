// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/public/first_run_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@interface AnimatedLensPromoTest : ChromeTestCase
@end

@implementation AnimatedLensPromoTest

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  // Show the First Run UI at startup.
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");
  // Enable the BestOfAppFRE feature with variant 2.
  config.additional_args.push_back("--enable-features=BestOfAppFRE:variant/2");
  // Disable the updated first run sequence so the Sign In and Default Browser
  // screen order is consistent.
  config.additional_args.push_back(
      "--disable-features=UpdatedFirstRunSequence");
  // Disable the animated default browser promo so the accessibility ID is
  // correct.
  config.additional_args.push_back(
      "--disable-features=AnimatedDefaultBrowserPromoInFRE");

  return config;
}

#pragma mark - Helpers

// Taps a promo button.
- (void)tapPromoButton:(id<GREYMatcher>)buttonMatcher {
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kPromoStyleScrollViewAccessibilityIdentifier);
  id<GREYAction> searchAction = grey_scrollInDirection(kGREYDirectionDown, 200);
  GREYElementInteraction* element =
      [[EarlGrey selectElementWithMatcher:buttonMatcher]
             usingSearchAction:searchAction
          onElementWithMatcher:scrollViewMatcher];
  [element performAction:grey_tap()];
}

- (void)skipSignInAndDefaultBrowser {
  // Skip sign in.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              first_run::kFirstRunSignInScreenAccessibilityIdentifier)];

  [self tapPromoButton:chrome_test_util::ButtonStackSecondaryButton()];

  // Skip default browser screen.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier)];

  [self tapPromoButton:chrome_test_util::ButtonStackSecondaryButton()];
}

#pragma mark - Tests

// Tests that metrics are recorded when the Lens promo is shown and the primary
// action is tapped.
- (void)testFirstRunStageHistogramMetricRecorded {
  // Set up the histogram tester.
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);

  // Verify that kAnimatedLensPromoStart is not yet recorded.
  chrome_test_util::GREYAssertErrorNil([MetricsAppInterface
       expectCount:0
         forBucket:static_cast<int>(first_run::kAnimatedLensPromoStart)
      forHistogram:@(first_run::kFirstRunStageHistogram)]);

  // Navigate to the Lens Animated Promo.
  [self skipSignInAndDefaultBrowser];

  // Wait for the Lens Animated Promo to appear.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              first_run::kAnimatedLensPromoAccessibilityIdentifier)];

  // Verify that kAnimatedLensPromoStart is recorded.
  chrome_test_util::GREYAssertErrorNil([MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(first_run::kAnimatedLensPromoStart)
      forHistogram:@(first_run::kFirstRunStageHistogram)]);

  // Verify that kAnimatedLensPromoCompletionWithAction is not yet recorded.
  chrome_test_util::GREYAssertErrorNil([MetricsAppInterface
       expectCount:0
         forBucket:static_cast<int>(
                       first_run::kAnimatedLensPromoCompletionWithAction)
      forHistogram:@(first_run::kFirstRunStageHistogram)]);

  // Tap the primary action button.
  [self tapPromoButton:chrome_test_util::ButtonStackPrimaryButton()];

  // Verify that kAnimatedLensPromoCompletionWithAction is recorded.
  chrome_test_util::GREYAssertErrorNil([MetricsAppInterface
       expectCount:1
         forBucket:static_cast<int>(
                       first_run::kAnimatedLensPromoCompletionWithAction)
      forHistogram:@(first_run::kFirstRunStageHistogram)]);
}

@end
