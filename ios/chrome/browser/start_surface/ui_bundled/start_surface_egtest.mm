// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "build/branding_buildflags.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/tab_resumption/public/tab_resumption_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_eg_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/test/tabs_egtest_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// The delay to wait for an element to appear before tapping on it.
constexpr base::TimeDelta kWaitElementTimeout = base::Seconds(4);

// Checks that the visibility of the tab resumption tile matches `should_show`.
void WaitUntilTabResumptionTileVisibleOrTimeout(bool should_show) {
  GREYCondition* tile_shown = [GREYCondition
      conditionWithName:@"Tab Resumption Module shown"
                  block:^BOOL {
                    NSError* error;
                    [[EarlGrey
                        selectElementWithMatcher:
                            grey_accessibilityID(
                                kMagicStackContentSuggestionsModuleTabResumptionAccessibilityIdentifier)]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];
  // Wait for the tile to be shown or timeout after kWaitForUIElementTimeout.
  BOOL success = [tile_shown
      waitWithTimeout:base::test::ios::kWaitForUIElementTimeout.InSecondsF()];
  if (should_show) {
    GREYAssertTrue(success, @"Tab Resumption Module did not appear.");
  } else {
    GREYAssertFalse(success, @"Tab Resumption Module appeared.");
  }
}

NSString* const kGroupName = @"1group";
const char kZeroSecondsThreshold[] = "0";
const char kThreeSecondsThreshold[] = "3";

}  // namespace

// Integration tests for the Start Surface user flows.
@interface StartSurfaceTestCase : ChromeTestCase
@end

@implementation StartSurfaceTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  config.additional_args.push_back("--test-ios-module-ranker=tab_resumption");
  config.additional_args.push_back("--mock-shopping-service=is-eligible,"
                                   "has-empty-price-tracked-bookmarks-results");

  if ([self isRunningTest:@selector(FLAKY_testShowTabGroupInGridOnStart)] ||
      [self isRunningTest:@selector
            (testDoNotShowTabGroupInGridOnStartInIncognitoMode)]) {
    config.features_enabled_and_params.push_back(
        {kShowTabGroupInGridOnStart,
         {{{kShowTabGroupInGridInactiveDurationInSeconds,
            kZeroSecondsThreshold}}}});
    config.features_enabled_and_params.push_back(
        {kStartSurface,
         {{{kReturnToStartSurfaceInactiveDurationInSeconds,
            kThreeSecondsThreshold}}}});
    return config;
  }

  config.features_enabled_and_params.push_back(
      {kStartSurface,
       {{{kReturnToStartSurfaceInactiveDurationInSeconds,
          kZeroSecondsThreshold}}}});

  return config;
}

- (void)setUp {
  [super setUp];
  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];
}

// Loads the first tab with an URL.
- (void)loadFirstTabURL {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL destinationURL = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationURL];
}

#pragma mark - Tests

// Tests that navigating to a page and restarting upon cold start, an NTP page
// is opened with the Return to Recent Tab tile.
// TODO(crbug.com/443695878): Test disabled on simulator.
#if TARGET_OS_SIMULATOR
#define MAYBE_testColdStartOpenStartSurface \
  DISABLED_testColdStartOpenStartSurface
#else
#define MAYBE_testColdStartOpenStartSurface testColdStartOpenStartSurface
#endif
- (void)MAYBE_testColdStartOpenStartSurface {
// TODO(crbug.com/40262902): Test is flaky on iPad device. Re-enable the test.
#if !TARGET_OS_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"This test is flaky on iPad device.");
  }
#endif
  [self loadFirstTabURL];

  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:[self appConfigurationForTestCase]];

  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
  // Assert NTP is visible by checking that the fake omnibox is here.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2UL,
                  @"Two tabs were expected to be open");
}

// Tests that navigating to a page and then backgrounding and foregrounding, an
// NTP page is opened.
// TODO(crbug.com/443695878): Test disabled on simulator.
#if TARGET_OS_SIMULATOR
#define MAYBE_testWarmStartOpenStartSurface \
  DISABLED_testWarmStartOpenStartSurface
#else
#define MAYBE_testWarmStartOpenStartSurface testWarmStartOpenStartSurface
#endif
- (void)MAYBE_testWarmStartOpenStartSurface {
  [self loadFirstTabURL];

  [ChromeEarlGrey
      waitForWebStateContainingText:"Anyone know any good pony jokes?"];

  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  // Give time for NTP to be fully loaded so all elements are accessible.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1.0));

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2UL,
                  @"Two tabs were expected to be open");
}

// Tests that navigating to a page and restarting upon cold start, an NTP page
// is opened with the Return to Recent Tab tile. Then, removing that last tab
// also removes the tile while that NTP is still being shown.
// TODO(crbug.com/441260657): Re-enable when fixed.
- (void)DISABLED_testRemoveRecentTabRemovesReturnToRecentTabTile {
  [self loadFirstTabURL];

  int non_start_tab_index = [ChromeEarlGrey indexOfActiveNormalTab];
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Give time for NTP to be fully loaded so all elements are accessible.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.5));
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2UL,
                  @"Two tabs were expected to be open");
  // Assert NTP is visible by checking that the fake omnibox is here.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  WaitUntilTabResumptionTileVisibleOrTimeout(true);

  NSUInteger nb_main_tab = [ChromeEarlGrey mainTabCount];
  [ChromeEarlGrey closeTabAtIndex:non_start_tab_index];

  ConditionBlock waitForTabToCloseCondition = ^{
    return [ChromeEarlGrey mainTabCount] == (nb_main_tab - 1);
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kWaitElementTimeout, waitForTabToCloseCondition),
             @"Waiting for tab to close");
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityLabel(
              kMagicStackContentSuggestionsModuleTabResumptionAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

#pragma mark - Multiwindow

// Tests that when a new window is being opened on iPad and the app enters split
// screen mode, Chrome will NOT force open a new tab page even when it does not
// have existing tabs.
- (void)testOpenNewWindowDoesNotReopenNTP {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  // Make sure there are no tabs on the current window.
  [ChromeEarlGrey closeAllExtraWindows];
  [ChromeEarlGrey closeAllTabs];
  // Open a new window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  // NTP should be opened in the new window, but not in the original one.
  [ChromeEarlGrey waitForMainTabCount:1 inWindowWithNumber:1];
  [ChromeEarlGrey waitForMainTabCount:0 inWindowWithNumber:0];
  [ChromeEarlGrey closeAllExtraWindows];
}

// Tests that the tab group in grid view is opened if Chrome is activated in the
// right time interval.
// TODO(crbug.com/462071614): Re-enable flaky test. This test is flaky due
// to devices possibly running under Stage Manager, hence the app never goes
// in the background. These tests expect the app to be backgrounding, and
// fail.
- (void)FLAKY_testShowTabGroupInGridOnStart {
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  chrome_test_util::CreateTabGroupAtIndex(0, kGroupName);

  // Open the group.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridGroupCellAtIndex(
                                          0)] performAction:grey_tap()];

  // Open the tab.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];

  // Simulate background then foreground activation.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Check that the tab group in grid view is open
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the tab group in grid view is not opened if Chrome is not
// activated in the right time interval.
- (void)testDoNotShowTabGroupInGridOnStart {
  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  chrome_test_util::CreateTabGroupAtIndex(0, kGroupName);

  // Open the group.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridGroupCellAtIndex(
                                          0)] performAction:grey_tap()];

  // Open the tab.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];

  // Simulate background then foreground activation.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Check that the tab group in grid view is not open.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the tab group in grid view is not opened if Chrome is activated in
// the right time interval but in Incognito mode.
- (void)testDoNotShowTabGroupInGridOnStartInIncognitoMode {
  [ChromeEarlGrey openNewIncognitoTab];

  [ChromeEarlGreyUI openTabGrid];

  // Create a tab group with an item at 0.
  chrome_test_util::CreateTabGroupAtIndex(0, kGroupName);

  // Open the group.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridGroupCellAtIndex(
                                          0)] performAction:grey_tap()];

  // Open the tab.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];

  // Simulate background then foreground activation.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Check that the tab group in grid view is not open.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the created NTP is ungrouped, even if a group was active when
// backgrounded.
- (void)testOpenNTPOutsideTheActiveGroupAfterFourHoursInBackground {
  [self loadFirstTabURL];

  [ChromeEarlGreyUI openTabGrid];

  chrome_test_util::CreateTabGroupAtIndex(0, kGroupName);

  // Open the group.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridGroupCellAtIndex(
                                          0)] performAction:grey_tap()];

  // Open the tab.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(0)]
      performAction:grey_tap()];

  GREYAssertEqual([ChromeEarlGrey mainTabCount], 1UL,
                  @"One tab was expected to be open");

  // Simulate background then foreground activation.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Assert NTP is visible by checking that the fake omnibox is here.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2UL,
                  @"Two tabs were expected to be open");

  [ChromeEarlGreyUI openTabGrid];

  // Check that the NTP is not in the group and visible in the Tab Grid view.
  [[EarlGrey selectElementWithMatcher:TabWithTitle(l10n_util::GetNSString(
                                          IDS_NEW_TAB_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the group has only 1 tab.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridGroupCellWithName(
                                   kGroupName, 1)]
      assertWithMatcher:grey_notNil()];
}

@end
