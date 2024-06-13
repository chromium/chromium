// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bring_android_tabs/ui_bundled/bring_android_tabs_test_session.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/bring_android_tabs_test_utils.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/constants.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/third_party/earl_grey2/src/CommonLib/GREYConstants.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

namespace {

// Returns the matcher for the Tab List From Android.
id<GREYMatcher> TabListFromAndroidMatcher() {
  return grey_accessibilityID(kBringAndroidTabsPromptTabListAXId);
}

// Returns the matcher for the Tab List From Android.
id<GREYMatcher> OpenButtonMatcher() {
  return grey_accessibilityID(kBringAndroidTabsPromptTabListOpenButtonAXId);
}

// Returns the matcher for the entry of "/pony.html" in the recent tabs panel.
id<GREYMatcher> PonyPageTitle() {
  return grey_allOf(
      grey_ancestor(TabListFromAndroidMatcher()),
      chrome_test_util::StaticTextWithAccessibilityLabel(@"ponies"),
      grey_sufficientlyVisible(), nil);
}

// Returns the matcher for the entry of "/chromium_logo_page.html" in the recent
// tabs panel.
id<GREYMatcher> ChromiumLogoPageTitle() {
  return grey_allOf(
      grey_ancestor(TabListFromAndroidMatcher()),
      chrome_test_util::StaticTextWithAccessibilityLabel(@"chromium logo"),
      grey_sufficientlyVisible(), nil);
}

// Triggers the tab list by selecting "review tabs" from the Bring Android Tabs
// confirmation alert prompt.
void TriggerTabList() {
  SignInViaFREWithHistorySyncEnabled(YES);
  [ChromeEarlGreyUI openTabGrid];
  VerifyConfirmationAlertPromptVisibility(YES);
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConfirmationAlertTertiaryActionAccessibilityIdentifier)]
      performAction:grey_tap()];
  VerifyTabListPromptVisibility(YES);
}

}  // namespace

// Test suite that tests user interactions with the Bring Android Tabs
// tab list.
@interface BringAndroidTabsTabListTestCase : ChromeTestCase

@end

@implementation BringAndroidTabsTabListTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  return GetConfiguration(/*is_android_switcher=*/YES);
}

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];
  if (![ChromeEarlGrey isIPadIdiom]) {
    GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
    AddSessionToFakeSyncServerFromTestServer(
        BringAndroidTabsTestSession::kRecentFromAndroidPhone,
        self.testServer->base_url());
  }
}

- (void)tearDown {
  CleanUp();
  [super tearDown];
}

// Tests that all tabs are opened when no tab is deselected from the list.
- (void)testOpenAllTabs {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  TriggerTabList();
  [[EarlGrey selectElementWithMatcher:OpenButtonMatcher()]
      performAction:grey_tap()];
  // New tab page already exists in the tab grid.
  [ChromeEarlGrey waitForMainTabCount:GetTabCountOnPrompt() + 1];
  VerifyTabListPromptVisibility(NO);
  VerifyThatPromptDoesNotShowOnRestart(self.testServer->base_url());
}

// Deselects a tab and tests that the remaining tab is opened.
- (void)testDeselectAndOpenTabs {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  TriggerTabList();
  // Deselect one of the tabs in the list.
  [[EarlGrey selectElementWithMatcher:PonyPageTitle()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OpenButtonMatcher()]
      performAction:grey_tap()];
  // Expected tab count accounts for the existing new tab page in the tab grid.
  [ChromeEarlGrey waitForMainTabCount:GetTabCountOnPrompt()];
  VerifyTabListPromptVisibility(NO);
  VerifyThatPromptDoesNotShowOnRestart(self.testServer->base_url());
}

// Tests that swiping down on the list dismisses the view and does not open any
// tabs.
- (void)testSwipeToDismiss {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  TriggerTabList();
  [[EarlGrey selectElementWithMatcher:TabListFromAndroidMatcher()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  // New tab page is in the tab grid.
  [ChromeEarlGrey waitForMainTabCount:1];
  VerifyTabListPromptVisibility(NO);
  VerifyThatPromptDoesNotShowOnRestart(self.testServer->base_url());
}

// Tests that tapping cancel dismisses the view and does not open any tabs.
- (void)testTapCancelToDismiss {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  TriggerTabList();
  id<GREYMatcher> cancelButtonMatcher =
      grey_accessibilityID(kBringAndroidTabsPromptTabListCancelButtonAXId);
  [[EarlGrey selectElementWithMatcher:cancelButtonMatcher]
      performAction:grey_tap()];
  // New tab page is in the tab grid.
  [ChromeEarlGrey waitForMainTabCount:1];
  VerifyTabListPromptVisibility(NO);
  VerifyThatPromptDoesNotShowOnRestart(self.testServer->base_url());
}

// Tests that the open tabs button is disabled when all of the tabs are
// deselected.
- (void)testOpenButtonDisabled {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  TriggerTabList();
  // Deselect all of the tabs in the list.
  [[EarlGrey selectElementWithMatcher:PonyPageTitle()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ChromiumLogoPageTitle()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OpenButtonMatcher()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];
  VerifyThatPromptDoesNotShowOnRestart(self.testServer->base_url());
}

@end
