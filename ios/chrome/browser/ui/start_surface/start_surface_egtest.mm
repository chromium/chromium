// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#include "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// The delay to wait for an element to appear before tapping on it.
const CGFloat kWaitElementTimeout = 2;
}

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Integration tests for the Start Surface user flows.
@interface StartSurfaceTestCase : ChromeTestCase
@end

@implementation StartSurfaceTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_enabled.push_back(kContentSuggestionsHeaderMigration);
  config.features_enabled.push_back(
      kContentSuggestionsUIViewControllerMigration);
  config.additional_args.push_back(
      std::string("--force-fieldtrial-params=StartSurface.ShrinkLogo:"
                  "ReturnToStartSurfaceInactiveDurationInSeconds/0"));
  return config;
}

// Tests that navigating to a page and restarting upon cold start, an NTP page
// is opened with the Return to Recent Tab tile.
// TODO(crbug.com/1323001): Fix flakiness.
- (void)DISABLED_testColdStartOpenStartSurface {
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
// TODO(crbug.com/1323001): Fix flakiness.
- (void)DISABLED_testWarmStartOpenStartSurface {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];

  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  [ChromeEarlGreyUI waitForAppToIdle];
  // Assert NTP is visible by checking that the fake omnibox is here.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
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
// TODO(crbug.com/1323001): Fix flakiness.
- (void)DISABLED_testRemoveRecentTabRemovesReturnToRecenTabTile {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];

  int non_start_tab_index = [ChromeEarlGrey indexOfActiveNormalTab];
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  [ChromeEarlGreyUI waitForAppToIdle];
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

@end
