// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "url/gurl.h"

namespace {

// Performs the assertion that the side swipe bubble appears and return the
// result.
void ExpectThatSideSwipeBubbleAppears() {
  // Disable scoped synchronization to perform checks with animation running.
  ScopedSynchronizationDisabler sync_disabler;
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(@"SideSwipeBubbleViewBubbleAXId")];
}

}  // namespace

@interface BubblePresenterTestCase : ChromeTestCase
@end

@implementation BubblePresenterTestCase

// Relaunch the app as a Safari switcher with IPH demo mode for `feature`.
- (void)relaunchWithIPHFeatureForSafariSwitcher:(NSString*)feature {
  // Enable the IPH Demo Mode feature to ensure the IPH triggers.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.additional_args.push_back(
      base::StringPrintf("--enable-features=%s:chosen_feature/"
                         "%s,IPHForSafariSwitcher",
                         feature_engagement::kIPHDemoMode.name,
                         base::SysNSStringToUTF8(feature).c_str()));
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

// Tests that the pull-to-refresh IPH when user taps the omnibox to reload the
// same page.
- (void)testPullToRefreshIPHAfterReloadFromOmnibox {
  [self relaunchWithIPHFeatureForSafariSwitcher:@"IPH_iOSPullToRefreshFeature"];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];
  [ChromeEarlGreyUI focusOmnibox];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];
  ExpectThatSideSwipeBubbleAppears();
}

// Tests that the pull-to-refresh IPH when user reloads the page using context
// menu.
- (void)testPullToRefreshIPHAfterReloadFromContextMenu {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no reload in context menu)");
  }
  [self relaunchWithIPHFeatureForSafariSwitcher:@"IPH_iOSPullToRefreshFeature"];

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];
  [ChromeEarlGreyUI reload];
  ExpectThatSideSwipeBubbleAppears();
}

@end
