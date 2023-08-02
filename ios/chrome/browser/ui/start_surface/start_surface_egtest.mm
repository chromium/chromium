// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
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
constexpr base::TimeDelta kWaitElementTimeout = base::Seconds(2);
}  // namespace

// Integration tests for the Start Surface user flows.
@interface StartSurfaceTestCase : ChromeTestCase
@end

@implementation StartSurfaceTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      "--enable-features=" + std::string(kStartSurface.name) + "<" +
      std::string(kStartSurface.name));
  config.additional_args.push_back(
      "--force-fieldtrials=" + std::string(kStartSurface.name) + "/Test");
  config.additional_args.push_back(
      "--force-fieldtrial-params=" + std::string(kStartSurface.name) +
      ".Test:" + std::string(kReturnToStartSurfaceInactiveDurationInSeconds) +
      "/" + "0");
  return config;
}

// Tests that navigating to a page and restarting upon cold start, an NTP page
// is opened with the Return to Recent Tab tile.
- (void)testColdStartOpenStartSurface {
// TODO(crbug.com/1430040): Test is flaky on iPad device. Re-enable the test.
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"This test is flaky on iPad device.");
  }
#endif
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];

  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:[self appConfigurationForTestCase]];

  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
  // Assert NTP is visible by checking that the fake omnibox is here.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2,
                  @"Two tabs were expected to be open");
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_RETURN_TO_RECENT_TAB_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that navigating to a page and then backgrounding and foregrounding, an
// NTP page is opened. Then, switching to the last tab and then back to the NTP
// does not show the Return to Recent Tab tile.
- (void)testWarmStartOpenStartSurface {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];

  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Give time for NTP to be fully loaded so all elements are accessible.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.5));
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_RETURN_TO_RECENT_TAB_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  int start_index = [ChromeEarlGrey indexOfActiveNormalTab];

  // Tap on Return to Recent Tab tile and switch back to NTP.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_RETURN_TO_RECENT_TAB_TITLE))]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForWebStateContainingText:"Anyone know any good pony jokes?"];
  [ChromeEarlGrey selectTabAtIndex:start_index];

  [ChromeEarlGrey
      waitForWebStateNotContainingText:"Anyone know any good pony jokes?"];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_RETURN_TO_RECENT_TAB_TITLE))]
      assertWithMatcher:grey_notVisible()];
}

// Tests that navigating to a page and restarting upon cold start, an NTP page
// is opened with the Return to Recent Tab tile. Then, removing that last tab
// also removes the tile while that NTP is still being shown.
- (void)testRemoveRecentTabRemovesReturnToRecenTabTile {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];

  int non_start_tab_index = [ChromeEarlGrey indexOfActiveNormalTab];
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Give time for NTP to be fully loaded so all elements are accessible.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.5));
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2,
                  @"Two tabs were expected to be open");
  // Assert NTP is visible by checking that the fake omnibox is here.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_RETURN_TO_RECENT_TAB_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  NSUInteger nb_main_tab = [ChromeEarlGrey mainTabCount];
  [ChromeEarlGrey closeTabAtIndex:non_start_tab_index];

  ConditionBlock waitForTabToCloseCondition = ^{
    return [ChromeEarlGrey mainTabCount] == (nb_main_tab - 1);
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kWaitElementTimeout, waitForTabToCloseCondition),
             @"Waiting for tab to close");
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_RETURN_TO_RECENT_TAB_TITLE))]
      assertWithMatcher:grey_notVisible()];
}

// Tests that navigating to a page and restarting upon cold start, an NTP page
// is opened with the Return to Recent Tab tile. Then, subsequently opening a
// new tab removes the Return To Recent Tab tile from both the new tab's NTP and
// the Start NTP.
- (void)testOpeningNewTabRemovesReturnToRecenTabTile {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];

  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Give time for NTP to be fully loaded so all elements are accessible.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.5));
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2,
                  @"Two tabs were expected to be open");
  // Assert NTP is visible by checking that the fake omnibox is here.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_RETURN_TO_RECENT_TAB_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Assert that Return to Recent Tab has been removed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_RETURN_TO_RECENT_TAB_TITLE))]
      assertWithMatcher:grey_notVisible()];

  // Close current tab to go back to the previous Start NTP.
  [ChromeEarlGrey closeCurrentTab];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_RETURN_TO_RECENT_TAB_TITLE))]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the Return To Recent Tab tile is removed after opening the tab
// grid (i.e. switching away from the Start Surface).
- (void)testReturnToRecenTabTileRemovedAfterOpeningTabGrid {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];

  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Give time for NTP to be fully loaded so all elements are accessible.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.5));
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2,
                  @"Two tabs were expected to be open");
  // Assert NTP is visible by checking that the fake omnibox is here.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_RETURN_TO_RECENT_TAB_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [ChromeEarlGreyUI openTabGrid];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(1)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_RETURN_TO_RECENT_TAB_TITLE))]
      assertWithMatcher:grey_notVisible()];
}

@end
