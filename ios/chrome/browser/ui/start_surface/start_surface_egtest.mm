// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/chrome/browser/ntp/home/features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_constants.h"
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
// NTP page is opened. Then, verifying that tapping on the tab resumption module
// switches back to the last tab.
// Disable due to https://crbug.com/1494900.
- (void)DISABLED_testWarmStartOpenStartSurface {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];

  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  // Give time for NTP to be fully loaded so all elements are accessible.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1.0));
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Rotate to landscape to Magic Stack can be scrollable for iPhone.
  if (![ChromeEarlGrey isIPadIdiom]) {
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                  error:nil];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
        performAction:grey_scrollInDirection(kGREYDirectionDown, 100)];
  }

  // Swipe over to the tab resumption module if needed.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(l10n_util::GetNSString(
                                       IDS_IOS_TAB_RESUMPTION_TITLE)),
                                   grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionRight, 343)
      onElementWithMatcher:grey_accessibilityID(
                               kMagicStackScrollViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on Return to Recent Tab tile and switch back to NTP.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTabResumptionViewIdentifier)]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForWebStateContainingText:"Anyone know any good pony jokes?"];
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

  // Rotate to landscape to Magic Stack can be scrollable for iPhone.
  if (![ChromeEarlGrey isIPadIdiom]) {
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                  error:nil];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
        performAction:grey_scrollInDirection(kGREYDirectionDown, 100)];
  }

  // Swipe over to the tab resumption module if needed.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(l10n_util::GetNSString(
                                       IDS_IOS_TAB_RESUMPTION_TITLE)),
                                   grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionRight, 343)
      onElementWithMatcher:grey_accessibilityID(
                               kMagicStackScrollViewAccessibilityIdentifier)]
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
                                   IDS_IOS_TAB_RESUMPTION_TITLE))]
      assertWithMatcher:grey_notVisible()];
}

@end
