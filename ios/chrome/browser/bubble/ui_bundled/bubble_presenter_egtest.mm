// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/gesture_in_product_help_view_egtest_utils.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/base_earl_grey_test_case_app_interface.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "url/gurl.h"

namespace {

using ::chrome_test_util::BackButton;
using ::chrome_test_util::ForwardButton;

// Bottom toolbar of of the tab view.
id<GREYMatcher> BottomToolbar() {
  return grey_kindOfClassName(@"SecondaryToolbarView");
}

// Open split screen. Should only be invoked for iPad.
void OpenSplitScreen() {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad without multiwindow support.");
  }
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [EarlGrey setRootMatcherForSubsequentInteractions:chrome_test_util::
                                                        WindowWithNumber(0)];
}

// Reload the current page from omnibox.
void ReloadFromOmnibox() {
  [ChromeEarlGreyUI focusOmnibox];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];
  [ChromeEarlGrey waitForPageToFinishLoading];
}

}  // namespace

@interface BubblePresenterTestCase : ChromeTestCase
@end

@implementation BubblePresenterTestCase

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
  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];
  [ChromeEarlGrey waitForPageToFinishLoading];
}

#pragma mark - Tests

- (void)setUp {
  [super setUp];
  MakeFirstRunRecent();
}

- (void)tearDown {
  [ChromeEarlGrey closeAllExtraWindows];
  [BaseEarlGreyTestCaseAppInterface enableFastAnimation];
  ResetFirstRunRecency();
  [super tearDown];
}

// Tests that the pull-to-refresh IPH is atttempted when user taps the omnibox
// to reload the same page, and disappears after the user navigates away.
- (void)testPullToRefreshIPHAfterReloadFromOmniboxAndDisappearsAfterNavigation {
  RelaunchWithIPHFeature(@"IPH_iOSPullToRefreshFeature",
                         /*safari_switcher=*/YES);
  if ([ChromeEarlGrey isIPadIdiom]) {
    OpenSplitScreen();
  }
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 =
      self.testServer->GetURL("/chromium_logo_page.html");
  [ChromeEarlGrey loadURL:destinationUrl1 inWindowWithNumber:0];
  [ChromeEarlGrey loadURL:destinationUrl2 inWindowWithNumber:0];
  ReloadFromOmnibox();
  AssertGestureIPHVisibleWithDismissAction(
      @"Pull to refresh IPH did not appear after reloading from omnibox.", ^{
        [[EarlGrey selectElementWithMatcher:BackButton()]
            performAction:grey_tap()];
      });
  AssertGestureIPHInvisible(
      @"Pull to refresh IPH still showed after user navigates to another page");
}

// Tests that the pull-to-refresh IPH is attempted when user reloads the page
// using context menu.
- (void)testPullToRefreshIPHAfterReloadFromContextMenuAndDisappearsOnSwitchTab {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad.");
  }
  RelaunchWithIPHFeature(@"IPH_iOSPullToRefreshFeature",
                         /*safari_switcher=*/YES);
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 =
      self.testServer->GetURL("/chromium_logo_page.html");
  [ChromeEarlGrey loadURL:destinationUrl1 inWindowWithNumber:0];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:destinationUrl2 inWindowWithNumber:0];
  // Reload using context menu.
  [ChromeEarlGreyUI reload];
  [ChromeEarlGrey waitForPageToFinishLoading];
  AssertGestureIPHVisibleWithDismissAction(
      @"Pull to refresh IPH did not appear after reloading from context menu.",
      ^{
        // Side swipe on the toolbar.
        [[EarlGrey selectElementWithMatcher:BottomToolbar()]
            performAction:grey_swipeSlowInDirection(kGREYDirectionRight)];
      });
  AssertGestureIPHInvisible(
      @"Pull to refresh IPH did not dismiss after changing tab.");
}

// Tests that the pull-to-refresh IPH is NOT attempted when page loading fails.
- (void)testPullToRefreshIPHShouldDisappearOnEnteringTabGrid {
  RelaunchWithIPHFeature(@"IPH_iOSPullToRefreshFeature",
                         /*safari_switcher=*/YES);
  if ([ChromeEarlGrey isIPadIdiom]) {
    OpenSplitScreen();
  }
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl inWindowWithNumber:0];
  ReloadFromOmnibox();
  AssertGestureIPHVisibleWithDismissAction(
      @"Pull to refresh IPH did not appear after reloading from omnibox.", ^{
        [ChromeEarlGreyUI openTabGrid];
      });
  AssertGestureIPHInvisible(
      @"Pull to refresh IPH still visible after going to tab grid.");
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  AssertGestureIPHInvisible(@"Pull to refresh IPH still visible after going to "
                            @"tab grid and coming back.");
  ExpectHistogramEmittedForIPHDismissal(
      IPHDismissalReasonType::kTappedOutsideIPHAndAnchorView);
}

// Tests that the pull-to-refresh IPH is NOT attempted when page loading fails.
- (void)testPullToRefreshIPHShouldNotShowOnPageLoadFail {
  RelaunchWithIPHFeature(@"IPH_iOSPullToRefreshFeature",
                         /*safari_switcher=*/YES);
  if ([ChromeEarlGrey isIPadIdiom]) {
    OpenSplitScreen();
  }
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl inWindowWithNumber:0];
  // Cut off server.
  GREYAssertTrue(self.testServer->ShutdownAndWaitUntilComplete(),
                 @"Server did not shut down.");
  ReloadFromOmnibox();
  AssertGestureIPHInvisible(
      @"Pull to refresh IPH still appeared despite loading fails.");
}

// Tests that the pull-to-refresh IPH is atttempted when user taps the omnibox
// to reload the same page, and disappears after the user navigates away.
- (void)testPullToRefreshIPHShouldNotShowOnRegularXRegular {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPhone.");
  }
  RelaunchWithIPHFeature(@"IPH_iOSPullToRefreshFeature",
                         /*safari_switcher=*/YES);
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/pony.html")];
  ReloadFromOmnibox();
  AssertGestureIPHInvisible(
      @"Pull to refresh IPH showed on regular x regular size class.");
}

// Tests that the swipe back/forward IPH is attempted on navigation, and
// disappears when user leaves the page.
- (void)testSwipeBackForwardIPHShowsOnNavigationAndHidesOnNavigation {
  RelaunchWithIPHFeature(@"IPH_iOSSwipeBackForward", /*safari_switcher=*/NO);
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 =
      self.testServer->GetURL("/chromium_logo_page.html");
  [ChromeEarlGrey loadURL:destinationUrl1];
  [ChromeEarlGrey loadURL:destinationUrl2];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  AssertGestureIPHVisibleWithDismissAction(
      @"Swipe back/forward IPH did not appear after tapping back button.", ^{
        [[EarlGrey selectElementWithMatcher:ForwardButton()]
            performAction:grey_tap()];
      });
  AssertGestureIPHInvisible(
      @"Swipe back/forward IPH still appeared after user left the page.");
  ExpectHistogramEmittedForIPHDismissal(
      IPHDismissalReasonType::kTappedOutsideIPHAndAnchorView);
}

// Tests that the pull-to-refresh IPH would be dismissed with the reason
// `kSwipedAsInstructedByGestureIPH` when the user pulls down on the IPH.
- (void)testPullToRefreshPerformAction {
  RelaunchWithIPHFeature(@"IPH_iOSPullToRefreshFeature",
                         /*safari_switcher=*/YES);
  if ([ChromeEarlGrey isIPadIdiom]) {
    OpenSplitScreen();
  }
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  // Trigger pull-to-refresh IPH.
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 =
      self.testServer->GetURL("/chromium_logo_page.html");
  [ChromeEarlGrey loadURL:destinationUrl1 inWindowWithNumber:0];
  [ChromeEarlGrey loadURL:destinationUrl2 inWindowWithNumber:0];
  ReloadFromOmnibox();
  AssertGestureIPHVisibleWithDismissAction(
      @"Pull to refresh IPH did not appear after reloading from omnibox.", ^{
        // Swipe down.
        SwipeIPHInDirection(kGREYDirectionDown, /*edge_swipe=*/NO);
      });
  AssertGestureIPHInvisible(
      @"Pull to refresh IPH should be dismissed after swiping down.");
  ExpectHistogramEmittedForIPHDismissal(
      IPHDismissalReasonType::kSwipedAsInstructedByGestureIPH);
}

// Tests that bi-directional swipe IPH shows when both forward and backward are
// navigatable, but only one-directional swipe shows when the user can only
// navigate back OR forward. The bi-directional swipe IPH takes longer to
// timeout.
- (void)testSwipeBackForwardIPHDirections {
  // Single direction swipe IPH takes 9s, while bi-direction swipe IPH takes
  // 12s; use a fixed wait time between the two to distinguish between the two
  // kinds of swipe IPHs.
  const base::TimeDelta waitTime = base::Seconds(11);
  RelaunchWithIPHFeature(@"IPH_iOSSwipeBackForward", /*safari_switcher=*/NO);
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 =
      self.testServer->GetURL("/chromium_logo_page.html");
  [ChromeEarlGrey loadURL:destinationUrl1];
  [ChromeEarlGrey loadURL:destinationUrl2];

  // Go back to destination URL 1.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  // Wait while animation runs.
  {
    ScopedSynchronizationDisabler sync_disabler;
    base::test::ios::SpinRunLoopWithMinDelay(waitTime);
  }
  AssertGestureIPHVisibleWithDismissAction(
      @"Bi-directional swipe back/forward IPH should still be visible.", nil);
  RelaunchWithIPHFeature(@"IPH_iOSSwipeBackForward", /*safari_switcher=*/NO);
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];
  // Go forward to destination URL 2.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  // Wait while animation runs.
  {
    ScopedSynchronizationDisabler sync_disabler;
    base::test::ios::SpinRunLoopWithMinDelay(waitTime);
  }
  AssertGestureIPHInvisible(
      @"One directional swipe back/forward IPH should not be visible.");
}

// Tests that opening a new tab hides the swipe back/forward IPH.
- (void)testSwipeBackForwardIPHHidesOnNewTabOpening {
  RelaunchWithIPHFeature(@"IPH_iOSSwipeBackForward", /*safari_switcher=*/NO);
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 =
      self.testServer->GetURL("/chromium_logo_page.html");
  [ChromeEarlGrey loadURL:destinationUrl1];
  [ChromeEarlGrey loadURL:destinationUrl2];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  AssertGestureIPHVisibleWithDismissAction(
      @"Swipe back/forward IPH did not appear after tapping back button.", ^{
        [ChromeEarlGrey openNewTab];
      });
  AssertGestureIPHInvisible(
      @"Swipe back/forward IPH still appeared after user opens a new tab.");
  ExpectHistogramEmittedForIPHDismissal(
      IPHDismissalReasonType::kTappedOutsideIPHAndAnchorView);
}

// Tests that the back/forward swipe IPH would be dismissed with the reason
// `kSwipedAsInstructedByGestureIPH` when the user swipes the page in the
// correct direction.
- (void)testSwipeBackForwardPerformAction {
  RelaunchWithIPHFeature(@"IPH_iOSSwipeBackForward", /*safari_switcher=*/NO);
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 =
      self.testServer->GetURL("/chromium_logo_page.html");
  [ChromeEarlGrey loadURL:destinationUrl1];
  [ChromeEarlGrey loadURL:destinationUrl2];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  AssertGestureIPHVisibleWithDismissAction(
      @"Swipe back/forward IPH did not appear after going back.", ^{
        SwipeIPHInDirection(kGREYDirectionLeft, /*edge_swipe=*/YES);
      });
  AssertGestureIPHInvisible(@"Swipe back/forward IPH should be dismissed after "
                            @"swiping in the right direction.");
  ExpectHistogramEmittedForIPHDismissal(
      IPHDismissalReasonType::kSwipedAsInstructedByGestureIPH);
}

// Tests that the swipe back/forward IPH would NOT show if the page load fails.
- (void)testSwipeBackForwardDoesNotShowWhenPageFails {
  RelaunchWithIPHFeature(@"IPH_iOSSwipeBackForward", /*safari_switcher=*/NO);
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

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
  AssertGestureIPHInvisible(
      @"Swipe back/forward IPH should not be visible when page fails to load.");
}

// Tests that the toolbar swipe IPH would be shown when and only when the user
// taps an adjacent tab.
- (void)testThatTappingAdjacentTabTriggersToolbarSwipeIPH {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (IPH is iPhone only)");
  }
  RelaunchWithIPHFeature(@"IPH_iOSSwipeToolbarToChangeTab",
                         /*safari_switcher=*/NO);
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  // Make sure three tabs are created.
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey loadURL:destinationUrl1];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:destinationUrl2];
  [ChromeEarlGrey openNewTab];
  // Switch to non-adjacent tab.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  AssertGestureIPHInvisible(@"Toolbar swipe IPH should not be visible when the "
                            @"user switches to an non-adjacent tab.");
  // Switch to adjacent tab.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(1)]
      performAction:grey_tap()];
  AssertGestureIPHVisibleWithDismissAction(
      @"Toolbar swipe IPH should be visible when the user switches to an "
      @"adjacent tab.",
      nil);
}

// Tests that the toolbar swipe IPH would be dismissed with the reason
// `kTappedClose` when the user taps "dismiss" on the IPH.
- (void)testShowToolbarSwipeIPHAndTapDismissButton {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (IPH is iPhone only)");
  }
  RelaunchWithIPHFeature(@"IPH_iOSSwipeToolbarToChangeTab",
                         /*safari_switcher=*/NO);
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  // Make sure two tabs are created.
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 =
      self.testServer->GetURL("/chromium_logo_page.html");
  [ChromeEarlGrey loadURL:destinationUrl1];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:destinationUrl2];
  // Switch to adjacent tab.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  AssertGestureIPHVisibleWithDismissAction(
      @"Toolbar swipe IPH should be visible when the user switches to an "
      @"adjacent tab.",
      ^{
        TapDismissButton();
      });
  AssertGestureIPHInvisible(
      @"IPH still displaying after the user taps the \"dismiss\" button.");
  ExpectHistogramEmittedForIPHDismissal(IPHDismissalReasonType::kTappedClose);
}

// Tests that the toolbar swipe IPH would be dismissed with the reason
// `kSwipedAsInstructedByGestureIPH` when the user swipes the toolbar in the
// correct direction.
- (void)testShowToolbarSwipeIPHAndPerformAction {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (IPH is iPhone only)");
  }
  RelaunchWithIPHFeature(@"IPH_iOSSwipeToolbarToChangeTab",
                         /*safari_switcher=*/NO);
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  // Make sure two tabs are created.
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 =
      self.testServer->GetURL("/chromium_logo_page.html");
  [ChromeEarlGrey loadURL:destinationUrl1];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:destinationUrl2];
  // Switch to adjacent tab.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  AssertGestureIPHVisibleWithDismissAction(
      @"Toolbar swipe IPH should be visible when the user switches to an "
      @"adjacent tab.",
      ^{
        // Swipe the toolbar in the wrong direction after it appears.
        [ChromeEarlGrey
            waitForUIElementToAppearWithMatcher:grey_allOf(BottomToolbar(),
                                                           grey_interactable(),
                                                           nil)];
        [[EarlGrey selectElementWithMatcher:BottomToolbar()]
            performAction:grey_swipeSlowInDirection(kGREYDirectionRight)];
      });
  AssertGestureIPHVisibleWithDismissAction(
      @"Toolbar swipe IPH should NOT be dismissed after swipe in the wrong "
      @"direction.",
      ^{
        [ChromeEarlGrey
            waitForUIElementToAppearWithMatcher:grey_allOf(BottomToolbar(),
                                                           grey_interactable(),
                                                           nil)];
        // Swipe the toolbar in the right direction.
        [[EarlGrey selectElementWithMatcher:BottomToolbar()]
            performAction:grey_swipeSlowInDirection(kGREYDirectionLeft)];
      });
  AssertGestureIPHInvisible(@"Toolbar swipe IPH should be dismissed after "
                            @"swipe in the right direction.");
  ExpectHistogramEmittedForIPHDismissal(
      IPHDismissalReasonType::kSwipedAsInstructedByGestureIPH);
}

// Tests that the toolbar swipe IPH would NOT be shown if the user has switched
// pages.
- (void)testThatToolbarSwipeIPHDoesNotShowAfterPageChange {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (IPH is iPhone only)");
  }
  RelaunchWithIPHFeature(@"IPH_iOSSwipeToolbarToChangeTab",
                         /*safari_switcher=*/NO);
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  // Make sure two tabs are created.
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 = self.testServer->GetURL("/destination.html");
  // Load two pages each in incognito and regular.
  [ChromeEarlGrey loadURL:destinationUrl1];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:destinationUrl2];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:destinationUrl1];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:destinationUrl2];
  // Switch to the "adjacent to active" tab on regular.
  [ChromeEarlGrey showTabSwitcher];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  AssertGestureIPHInvisible(
      @"Toolbar swipe IPH should not be visible when the "
      @"user switches to an adjacent tab after changing page.");
}

// Tests that the toolbar swipe IPH would be dismissed with the reason
// `kTappedOutsideIPHAndAnchorView` when the user leaves the page using other
// means.
- (void)testShowToolbarSwipeIPHAndLeavePage {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (IPH is iPhone only)");
  }
  RelaunchWithIPHFeature(@"IPH_iOSSwipeToolbarToChangeTab",
                         /*safari_switcher=*/NO);
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  // Make sure two tabs are created.
  const GURL destinationUrl1 = self.testServer->GetURL("/pony.html");
  const GURL destinationUrl2 = self.testServer->GetURL("/destination.html");
  const GURL destinationUrl3 =
      self.testServer->GetURL("/chromium_logo_page.html");
  // Load two pages so that the user can tap back button.
  [ChromeEarlGrey loadURL:destinationUrl1];
  [ChromeEarlGrey loadURL:destinationUrl2];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:destinationUrl3];
  // Switch to adjacent tab.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];
  // Checks visibility and tap "back" button to dismiss the IPH.
  AssertGestureIPHVisibleWithDismissAction(
      @"Toolbar swipe IPH should be visible when the user switches to an "
      @"adjacent tab.",
      ^{
        [ChromeEarlGrey
            waitForUIElementToAppearWithMatcher:grey_allOf(BackButton(),
                                                           grey_interactable(),
                                                           nil)];
        [[EarlGrey selectElementWithMatcher:BackButton()]
            performAction:grey_tap()];
      });
  AssertGestureIPHInvisible(
      @"Toolbar swipe IPH should be dismissed after leaving the page.");
  ExpectHistogramEmittedForIPHDismissal(
      IPHDismissalReasonType::kTappedOutsideIPHAndAnchorView);
}

@end
