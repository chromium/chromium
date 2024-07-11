// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "build/branding_buildflags.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_constants.h"
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
  config.additional_args.push_back("--test-ios-module-ranker=tab_resumption");
  // Tests need to be adapted to make sure local server tabs appear with TR2.
  config.features_disabled.push_back(kTabResumption2);
  return config;
}

- (void)setUp {
  [super setUp];
  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];
}

// Tests that navigating to a page and restarting upon cold start, an NTP page
// is opened with the Return to Recent Tab tile.
- (void)testColdStartOpenStartSurface {
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
}

// Tests that navigating to a page and then backgrounding and foregrounding, an
// NTP page is opened.
- (void)testWarmStartOpenStartSurface {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];
  [ChromeEarlGrey
      waitForWebStateContainingText:"Anyone know any good pony jokes?"];

  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  // Give time for NTP to be fully loaded so all elements are accessible.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1.0));

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
  GREYAssertEqual([ChromeEarlGrey mainTabCount], 2,
                  @"Two tabs were expected to be open");
}

// Tests that navigating to a page and restarting upon cold start, an NTP page
// is opened with the Return to Recent Tab tile. Then, removing that last tab
// also removes the tile while that NTP is still being shown.
- (void)testRemoveRecentTabRemovesReturnToRecentTabTile {
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

@end
