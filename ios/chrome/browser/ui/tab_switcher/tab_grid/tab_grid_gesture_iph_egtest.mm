// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/gesture_in_product_help_view_egtest_utils.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/base_earl_grey_test_case_app_interface.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

using ::chrome_test_util::IncognitoTabGrid;
using ::chrome_test_util::TabGridIncognitoTabsPanelButton;
using ::chrome_test_util::TabGridNormalModePageControl;
using ::chrome_test_util::TabGridSearchTabsButton;

}  // namespace

// Test cases for gesture in-product help views on the tab grid.
@interface TabGridGestureInProductHelpTestCase : ChromeTestCase

@end

@implementation TabGridGestureInProductHelpTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.iph_feature_enabled =
      feature_engagement::kIPHiOSTabGridSwipeRightForIncognito.name;
  // Users with first run recency of less than 60 are considered new users.
  config.additional_args.push_back("-FirstRunRecency");
  config.additional_args.push_back("59");
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

- (void)setUp {
  // Non-startup tests closes all tabs in set up, exposing the tab grid and show
  // the IPH before test body gets executed. Working around but marking it as a
  // startup test.
  [[self class] testForStartup];
  [super setUp];
  MakeFirstRunRecent();
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  [[self class] removeAnyOpenMenusAndInfoBars];
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];
  [ChromeEarlGreyUI openTabGrid];
  // Wait for tab grid animation to complete; value is the animation duration
  // for `PointZoomTransitionAnimation`.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.2));
}

- (void)tearDown {
  [BaseEarlGreyTestCaseAppInterface enableFastAnimation];
  ResetFirstRunRecency();
  [super tearDown];
}

// Tests that the swipe IPH can be shown and dismissed on timeout.
- (void)testSwipeForIncognitoGesturalIPHAndTimeout {
  AssertGestureIPHVisibleWithDismissAction(
      @"IPH doesn't show after the user taps to go to incognito twice.", ^{
        // Wait while animation runs until timeout (Timeout value is 9s.)
        base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(9.5));
      });
  // Should NOT show IPH after timeout.
  AssertGestureIPHInvisible(@"IPH still displaying after the timeout.");
  ExpectHistogramEmittedForIPHDismissal(IPHDismissalReasonType::kTimedOut);
}

// Tests that the swipe IPH can be shown and the user could tap the "dismiss"
// button to remove it.
- (void)testSwipeForIncognitoGesturalIPHAndTapDismissButton {
  AssertGestureIPHVisibleWithDismissAction(
      @"IPH doesn't show after the user taps to go to incognito twice.", nil);
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.5));
  TapDismissButton();
  // Should NOT show IPH after dismissal.
  AssertGestureIPHInvisible(
      @"IPH still displaying after the user taps the \"dismiss\" button.");
  ExpectHistogramEmittedForIPHDismissal(IPHDismissalReasonType::kTappedClose);
}

// Tests that the swipe IPH can be shown and dismissed on mode change.
- (void)testSwipeForIncognitoGesturalIPHAndChangeMode {
  AssertGestureIPHVisibleWithDismissAction(
      @"IPH doesn't show after the user taps to go to incognito twice.", ^{
        [[EarlGrey selectElementWithMatcher:TabGridSearchTabsButton()]
            performAction:grey_tap()];
      });
  // Should NOT show IPH after mode change.
  AssertGestureIPHInvisible(
      @"IPH still displaying after the user changes mode.");
  ExpectHistogramEmittedForIPHDismissal(
      IPHDismissalReasonType::kTappedOutsideIPHAndAnchorView);
}

// Tests that the swipe IPH can be shown and dismissed on tab grid page change.
- (void)testSwipeForIncognitoGesturalIPHAndChangePage {
  AssertGestureIPHVisibleWithDismissAction(
      @"IPH doesn't show after the user taps to go to incognito twice.", ^{
        [[EarlGrey selectElementWithMatcher:TabGridIncognitoTabsPanelButton()]
            performAction:grey_tap()];
      });
  // Go back to regular page.
  [[EarlGrey selectElementWithMatcher:TabGridNormalModePageControl()]
      performAction:grey_tap()];
  AssertGestureIPHInvisible(@"IPH still displaying after the user goes to "
                            @"incognito mode and comes back.");
  ExpectHistogramEmittedForIPHDismissal(
      IPHDismissalReasonType::kTappedOutsideIPHAndAnchorView);
}

// Tests that swiping in the right direction dismisses the IPH and scrolls the
// tab grid to incognito.
- (void)testSwipeRightToDismissIPHAndGoToIncognito {
  AssertGestureIPHVisibleWithDismissAction(
      @"IPH doesn't show after the user taps to go to incognito twice.", ^{
        SwipeIPHInDirection(kGREYDirectionRight, /*edge_swipe=*/NO);
      });
  [[EarlGrey selectElementWithMatcher:IncognitoTabGrid()]
      assertWithMatcher:grey_sufficientlyVisible()];
  ExpectHistogramEmittedForIPHDismissal(
      IPHDismissalReasonType::kSwipedAsInstructedByGestureIPH);
}

// Tests that swiping in the wrong direction does nothing.
- (void)testSwipeLeftDoesNotDismissIPHAndGoToIncognito {
  AssertGestureIPHVisibleWithDismissAction(
      @"IPH doesn't show after the user taps to go to incognito twice.", ^{
        SwipeIPHInDirection(kGREYDirectionLeft, /*edge_swipe=*/NO);
      });
  // The IPH should have auto-dismissed by now; verify that the user is NOT
  // viewing in incognito.
  [[EarlGrey selectElementWithMatcher:IncognitoTabGrid()]
      assertWithMatcher:grey_notVisible()];
}

@end
