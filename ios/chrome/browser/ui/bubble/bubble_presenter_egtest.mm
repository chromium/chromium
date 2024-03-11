// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/bubble/gesture_iph/gesture_in_product_help_view_egtest_utils.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/base_earl_grey_test_case_app_interface.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "url/gurl.h"

namespace {
using ::chrome_test_util::BackButton;
using ::chrome_test_util::ForwardButton;
}  // namespace

@interface BubblePresenterTestCase : ChromeTestCase
@end

@implementation BubblePresenterTestCase

// Relaunch the app as a Safari switcher with IPH demo mode for `feature`.
- (void)relaunchWithIPHFeatureForSafariSwitcher:(NSString*)feature {
  // Enable the flag to ensure the IPH triggers.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.iph_feature_enabled = base::SysNSStringToUTF8(feature);
  config.additional_args.push_back("--enable-features=IPHForSafariSwitcher");
  // Force the conditions that allow the iph to show.
  config.additional_args.push_back("-ForceExperienceForDeviceSwitcher");
  config.additional_args.push_back("SyncedAndFirstDevice");
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

// Open a random url from omnibox. `isAfterNewAppLaunch` is used for deciding
// whether the step of tapping the fake omnibox is needed.
- (void)openURLFromOmniboxWithIsAfterNewAppLaunch:(BOOL)isAfterNewAppLaunch {
  if (isAfterNewAppLaunch) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
        performAction:grey_tap()];
    [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                        chrome_test_util::Omnibox()];
  }

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(@"chrome://version")];
  // TODO(crbug.com/1454516): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];
}

#pragma mark - Tests

- (void)tearDown {
  [BaseEarlGreyTestCaseAppInterface enableFastAnimation];
  [super tearDown];
}

// Tests that the New Tab IPH can be displayed when opening an URL from omnibox.
// TODO(crbug.com/1471222): Test is flaky on device. Re-enable the test.
#if !TARGET_OS_SIMULATOR
#define MAYBE_testNewTabIPH FLAKY_testNewTabIPH
#else
#define MAYBE_testNewTabIPH testNewTabIPH
#endif
- (void)MAYBE_testNewTabIPH {
  [self relaunchWithIPHFeatureForSafariSwitcher:
            @"IPH_iOSNewTabToolbarItemFeature"];
  [self openURLFromOmniboxWithIsAfterNewAppLaunch:YES];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              @"BubbleViewLabelIdentifier")];
}

// Tests that the Tab Grid IPH can be displayed when opening a new tab and there
// are multiple tabs.
// TODO(crbug.com/1471222): Test is flaky on device. Re-enable the test.
#if !TARGET_OS_SIMULATOR
#define MAYBE_testTabGridIPH FLAKY_testTabGridIPH
#else
#define MAYBE_testTabGridIPH testTabGridIPH
#endif
- (void)MAYBE_testTabGridIPH {
  [self relaunchWithIPHFeatureForSafariSwitcher:
            @"IPH_iOSTabGridToolbarItemFeature"];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              @"BubbleViewLabelIdentifier")];
}

// Tests that the pull-to-refresh IPH is atttempted when user taps the omnibox
// to reload the same page, and disappears after the user navigates away.
// TODO(crbug.com/329078389): This test is flaky on simulator.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testPullToRefreshIPHAfterReloadFromOmniboxAndDisappearsAfterNavigation \
  FLAKY_testPullToRefreshIPHAfterReloadFromOmniboxAndDisappearsAfterNavigation
#else
#define MAYBE_testPullToRefreshIPHAfterReloadFromOmniboxAndDisappearsAfterNavigation \
  testPullToRefreshIPHAfterReloadFromOmniboxAndDisappearsAfterNavigation
#endif
- (void)
    MAYBE_testPullToRefreshIPHAfterReloadFromOmniboxAndDisappearsAfterNavigation {
  [self relaunchWithIPHFeatureForSafariSwitcher:@"IPH_iOSPullToRefreshFeature"];
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 =
      self.testServer->GetURL("/chromium_logo_page.html");
  [ChromeEarlGrey loadURL:destinationUrl1];
  [ChromeEarlGrey loadURL:destinationUrl2];
  [ChromeEarlGreyUI focusOmnibox];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];
  BOOL appearance = HasGestureIPHAppeared();
  {
    // Disable scoped synchronization to tap button.
    ScopedSynchronizationDisabler sync_disabler;
    GREYAssertTrue(
        appearance,
        @"Pull to refresh IPH did not appear after reloading from omnibox.");
    [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  }
  appearance = HasGestureIPHAppeared();
  GREYAssertFalse(
      appearance,
      @"Pull to refresh IPH still showed after user navigates to another page");
}

// Tests that the pull-to-refresh IPH is attempted when user reloads the page
// using context menu.
- (void)testPullToRefreshIPHAfterReloadFromContextMenuAndDisappearsOnSwitchTab {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no reload in context menu)");
  }
  [self relaunchWithIPHFeatureForSafariSwitcher:@"IPH_iOSPullToRefreshFeature"];
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 =
      self.testServer->GetURL("/chromium_logo_page.html");
  [ChromeEarlGrey loadURL:destinationUrl1];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:destinationUrl2];
  // Reload using context menu.
  [ChromeEarlGreyUI reload];
  BOOL appearance = HasGestureIPHAppeared();
  {
    // Disable scoped synchronization.
    ScopedSynchronizationDisabler sync_disabler;
    GREYAssertTrue(appearance, @"Pull to refresh IPH did not appear "
                               @"after reloading from context menu.");
    // Side swipe on the toolbar.
    [[EarlGrey
        selectElementWithMatcher:grey_kindOfClassName(@"SecondaryToolbarView")]
        performAction:grey_swipeSlowInDirection(kGREYDirectionRight)];
  }
  appearance = HasGestureIPHAppeared();
  GREYAssertFalse(appearance,
                  @"Pull to refresh IPH did not dismiss after changing tab.");
}

// Tests that the pull-to-refresh IPH is NOT attempted when page loading fails.
- (void)testPullToRefreshIPHShouldDisappearOnEnteringTabGrid {
  [self relaunchWithIPHFeatureForSafariSwitcher:@"IPH_iOSPullToRefreshFeature"];
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];
  [ChromeEarlGreyUI focusOmnibox];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];
  BOOL appearance = HasGestureIPHAppeared();
  {
    // Disable scoped synchronization to tap button.
    ScopedSynchronizationDisabler sync_disabler;
    GREYAssertTrue(
        appearance,
        @"Pull to refresh IPH did not appear after reloading from omnibox.");
    [ChromeEarlGreyUI openTabGrid];
  }
  appearance = HasGestureIPHAppeared();
  GREYAssertFalse(
      appearance,
      @"Pull to refresh IPH still visible after going to tab grid.");
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  appearance = HasGestureIPHAppeared();
  GREYAssertFalse(appearance, @"Pull to refresh IPH still visible after going "
                              @"to tab grid and coming back.");
}

// Tests that the pull-to-refresh IPH is NOT attempted when page loading fails.
- (void)testPullToRefreshIPHShouldNotShowOnPageLoadFail {
  [self relaunchWithIPHFeatureForSafariSwitcher:@"IPH_iOSPullToRefreshFeature"];
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];
  // Cut off server.
  GREYAssertTrue(self.testServer->ShutdownAndWaitUntilComplete(),
                 @"Server did not shut down.");
  [ChromeEarlGreyUI focusOmnibox];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];
  BOOL appearance = HasGestureIPHAppeared();
  GREYAssertFalse(appearance,
                  @"Pull to refresh IPH still appeared despite loading fails.");
}

// Tests that the swipe back/forward IPH is attempted on navigation, and
// disappears when user leaves the page.
// TODO(crbug.com/328732643): This test is flaky on simulator.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testSwipeBackForwardIPHShowsOnNavigationAndHidesOnNavigation \
  FLAKY_testSwipeBackForwardIPHShowsOnNavigationAndHidesOnNavigation
#else
#define MAYBE_testSwipeBackForwardIPHShowsOnNavigationAndHidesOnNavigation \
  testSwipeBackForwardIPHShowsOnNavigationAndHidesOnNavigation
#endif
- (void)MAYBE_testSwipeBackForwardIPHShowsOnNavigationAndHidesOnNavigation {
  [self relaunchWithIPHFeatureForSafariSwitcher:@"IPH_iOSSwipeBackForward"];
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 =
      self.testServer->GetURL("/chromium_logo_page.html");
  [ChromeEarlGrey loadURL:destinationUrl1];
  [ChromeEarlGrey loadURL:destinationUrl2];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  BOOL appearance = HasGestureIPHAppeared();
  {
    // Disable scoped synchronization to tap button.
    ScopedSynchronizationDisabler sync_disabler;
    GREYAssertTrue(
        appearance,
        @"Swipe back/forward IPH did not appear after tapping back button.");
    [[EarlGrey selectElementWithMatcher:ForwardButton()]
        performAction:grey_tap()];
  }
  appearance = HasGestureIPHAppeared();
  GREYAssertFalse(
      appearance,
      @"Swipe back/forward IPH still appeared after user left the page.");
}

// Tests that bi-directional swipe IPH shows when both forward and backward are
// navigatable, but only one-directional swipe shows when the user can only
// navigate back OR forward. The bi-directional swipe IPH takes longer to
// timeout.
- (void)testSwipeBackForwardIPHDirections {
  // Single direction swipe IPH takes 9s, while bi-direction swipe IPH takes
  // 12s; use a fixed wait time between the two to distinguish between the two
  // kinds of swipe IPHs.
  const base::TimeDelta waitTime = base::Seconds(11.5);
  [self relaunchWithIPHFeatureForSafariSwitcher:@"IPH_iOSSwipeBackForward"];
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 =
      self.testServer->GetURL("/chromium_logo_page.html");
  [ChromeEarlGrey loadURL:destinationUrl1];
  [ChromeEarlGrey loadURL:destinationUrl2];

  // Go back to destination URL 1.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  {
    // Wait while animation runs.
    ScopedSynchronizationDisabler sync_disabler;
    base::test::ios::SpinRunLoopWithMinDelay(waitTime);
  }
  BOOL appearance = HasGestureIPHAppeared();
  GREYAssertTrue(
      appearance,
      @"Bi-directional swipe back/forward IPH should still be visible.");

  [self relaunchWithIPHFeatureForSafariSwitcher:@"IPH_iOSSwipeBackForward"];
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];
  // Go forward to destination URL 2.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  {
    // Wait while animation runs.
    ScopedSynchronizationDisabler sync_disabler;
    base::test::ios::SpinRunLoopWithMinDelay(waitTime);
  }
  appearance = HasGestureIPHAppeared();
  GREYAssertFalse(
      appearance,
      @"One directional swipe back/forward IPH should not be visible.");
}

// Tests that opening a new tab hides the swipe back/forward IPH.
- (void)testSwipeBackForwardIPHHidesOnNewTabOpening {
  [self relaunchWithIPHFeatureForSafariSwitcher:@"IPH_iOSSwipeBackForward"];
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 =
      self.testServer->GetURL("/chromium_logo_page.html");
  [ChromeEarlGrey loadURL:destinationUrl1];
  [ChromeEarlGrey loadURL:destinationUrl2];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  BOOL appearance = HasGestureIPHAppeared();
  {
    // Disable scoped synchronization to tap button during animation.
    ScopedSynchronizationDisabler sync_disabler;
    GREYAssertTrue(
        appearance,
        @"Swipe back/forward IPH did not appear after tapping back button.");
    [ChromeEarlGrey openNewTab];
  }
  appearance = HasGestureIPHAppeared();
  GREYAssertFalse(
      appearance,
      @"Swipe back/forward IPH still appeared after user opens a new tab.");
}

// Tests that the swipe back/forward IPH would NOT show if the page load fails.
- (void)testSwipeBackForwardDoesNotShowWhenPageFails {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 =
      self.testServer->GetURL("/chromium_logo_page.html");
  [ChromeEarlGrey loadURL:destinationUrl1];
  [ChromeEarlGrey loadURL:destinationUrl2];
  [ChromeEarlGrey purgeCachedWebViewPages];

  // Cut off server and go back to destination URL 1.
  GREYAssertTrue(self.testServer->ShutdownAndWaitUntilComplete(),
                 @"Server did not shut down.");
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  BOOL appearance = HasGestureIPHAppeared();
  GREYAssertFalse(
      appearance,
      @"Swipe back/forward IPH should not be visible when page fails to load.");
}

@end
