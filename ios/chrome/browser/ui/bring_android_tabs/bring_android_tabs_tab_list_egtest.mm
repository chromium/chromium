// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bring_android_tabs/bring_android_tabs_app_interface.h"
#import "ios/chrome/browser/ui/bring_android_tabs/bring_android_tabs_test_utils.h"
#import "ios/chrome/browser/ui/bring_android_tabs/constants.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/third_party/earl_grey2/src/CommonLib/GREYConstants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns the matcher for the Tab List From Android.
id<GREYMatcher> TabListFromAndroidMatcher() {
  return grey_accessibilityID(kBringAndroidTabsPromptTabListAXId);
}

// Returns the matcher for the Tab List From Android.
id<GREYMatcher> OpenButtonMatcher() {
  return grey_accessibilityID(kBringAndroidTabsPromptTabListOpenButtonAXId);
}

// Returns the matcher for the entry of the page in the recent tabs panel.
id<GREYMatcher> GoogleTestPageTitle() {
  return grey_allOf(
      grey_ancestor(TabListFromAndroidMatcher()),
      chrome_test_util::StaticTextWithAccessibilityLabel(@"Google"),
      grey_sufficientlyVisible(), nil);
}

// Returns the matcher for the entry of the page in the recent tabs panel.
id<GREYMatcher> ChromiumTestPageTitle() {
  return grey_allOf(grey_ancestor(TabListFromAndroidMatcher()),
                    chrome_test_util::StaticTextWithAccessibilityLabel(@"Home"),
                    grey_sufficientlyVisible(), nil);
}

// Triggers the tab list by selecting "review tabs" from the Bring Android Tabs
// confirmation alert prompt.
void TriggerTabList() {
  CompleteFREWithSyncEnabled(YES);
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
  return GetConfiguration(/*is_android_switcher=*/YES,
                          /*show_bottom_message=*/NO);
}

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];
  if (![ChromeEarlGrey isIPadIdiom]) {
    [BringAndroidTabsAppInterface
        addSessionToFakeSyncServer:BringAndroidTabsAppInterfaceForeignSession::
                                       kRecentFromAndroidPhone];
  }
}

- (void)tearDown {
  CleanUp();
  [super tearDown];
}

// Tests that all tabs are opened when no tab is deselected from the list.
// TODO(crbug.com/1450831): The test is flaky.
- (void)FLAKY_testOpenAllTabs {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  TriggerTabList();
  [[EarlGrey selectElementWithMatcher:OpenButtonMatcher()]
      performAction:grey_tap()];
  int expectedTabCountFromDistantSessions = [BringAndroidTabsAppInterface
      tabsCountForSession:BringAndroidTabsAppInterfaceForeignSession::
                              kRecentFromAndroidPhone];
  // New tab page already exists in the tab grid.
  [ChromeEarlGrey waitForMainTabCount:expectedTabCountFromDistantSessions + 1];
  VerifyTabListPromptVisibility(NO);
  VerifyThatPromptDoesNotShowOnRestart(/*bottom_message=*/NO);
}

// Deselects a tab and tests that the remaining tab is opened.
- (void)testDeselectAndOpenTabs {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  TriggerTabList();
  // Deselect one of the tabs in the list.
  [[EarlGrey selectElementWithMatcher:ChromiumTestPageTitle()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OpenButtonMatcher()]
      performAction:grey_tap()];
  // Expected tab count accounts for the existing new tab page in the tab grid.
  int expectedTabCountFromDistantSessions = [BringAndroidTabsAppInterface
      tabsCountForSession:BringAndroidTabsAppInterfaceForeignSession::
                              kRecentFromAndroidPhone];
  [ChromeEarlGrey waitForMainTabCount:expectedTabCountFromDistantSessions];
  VerifyTabListPromptVisibility(NO);
  VerifyThatPromptDoesNotShowOnRestart(/*bottom_message=*/NO);
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
  VerifyThatPromptDoesNotShowOnRestart(/*bottom_message=*/NO);
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
  VerifyThatPromptDoesNotShowOnRestart(/*bottom_message=*/NO);
}

// Tests that the open tabs button is disabled when all of the tabs are
// deselected.
- (void)testOpenButtonDisabled {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  TriggerTabList();
  // Deselect all of the tabs in the list.
  [[EarlGrey selectElementWithMatcher:ChromiumTestPageTitle()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:GoogleTestPageTitle()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OpenButtonMatcher()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];
  VerifyThatPromptDoesNotShowOnRestart(/*bottom_message=*/NO);
}

@end
