// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bring_android_tabs/bring_android_tabs_test_session.h"
#import "ios/chrome/browser/ui/bring_android_tabs/bring_android_tabs_test_utils.h"
#import "ios/chrome/browser/ui/bring_android_tabs/constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

// Test suite that tests user interactions with the Bring Android Tabs bottom
// message prompt.
@interface BringAndroidTabsBottomMessageTestCase : ChromeTestCase

@end

@implementation BringAndroidTabsBottomMessageTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  return GetConfiguration(/*is_android_switcher=*/YES,
                          /*show_bottom_message=*/YES);
}

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];
  if (![ChromeEarlGrey isIPadIdiom]) {
    GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
    AddSessionToFakeSyncServerFromTestServer(
        BringAndroidTabsTestSession::kRecentFromAndroidPhone,
        self.testServer->base_url());
    CompleteFREWithSyncEnabled(YES);
  }
}

- (void)tearDown {
  CleanUp();
  [super tearDown];
}

// Tests that the user can close the bottom message prompt by tapping the
// "close" button. The prompt would not be shown again when the user leaves the
// tab grid and comes back, or restarts.
- (void)testClose {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  [ChromeEarlGreyUI openTabGrid];
  VerifyBottomMessagePromptVisibility(YES);
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBringAndroidTabsPromptBottomMessageCloseButtonAXId)]
      performAction:grey_tap()];
  VerifyBottomMessagePromptVisibility(NO);
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openTabGrid];
  VerifyBottomMessagePromptVisibility(NO);
  VerifyThatPromptDoesNotShowOnRestart(
      /*bottom_message=*/YES, self.testServer->base_url());
}

// Tests that the user can review the list of Android tabs by tapping the
// "review" button on the bottom message prompt. Afterwards, the prompt would
// not be shown again after the user restarts.
- (void)testReview {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  [ChromeEarlGreyUI openTabGrid];
  VerifyBottomMessagePromptVisibility(YES);
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBringAndroidTabsPromptBottomMessageReviewButtonAXId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBringAndroidTabsPromptTabListAXId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  VerifyThatPromptDoesNotShowOnRestart(
      /*bottom_message=*/YES, self.testServer->base_url());
}

// Tests that the if the user leaves the tab grid without interacting with the
// bottom message prompt, the prompt would still be visible when they comes back
// to the tab grid. However, if the user relaunches the app and go to tab grid,
// the prompt would not be shown.
- (void)testLeaveTabGridThenComeBackThenRestart {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  [ChromeEarlGreyUI openTabGrid];
  VerifyBottomMessagePromptVisibility(YES);
  // Leave tab and come back.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openTabGrid];
  VerifyBottomMessagePromptVisibility(YES);
  VerifyThatPromptDoesNotShowOnRestart(
      /*bottom_message=*/YES, self.testServer->base_url());
}

// Tests that the bottom message only shows on regular tab grid.
- (void)testThatPromptOnlyShowInRegularTabGrid {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  [ChromeEarlGreyUI openTabGrid];
  VerifyBottomMessagePromptVisibility(YES);
  // Go to incognito tab grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];
  VerifyBottomMessagePromptVisibility(NO);
  // Go to recent tabs.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];
  VerifyBottomMessagePromptVisibility(NO);
  // Come back.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];
  VerifyBottomMessagePromptVisibility(YES);
}

// Tests that the bottom message stays on the regular tab grid no matter now
// many new tabs are opened or closed.
- (void)testThatPromptStaysWhenUserClosesTabs {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  [ChromeEarlGreyUI openTabGrid];
  VerifyBottomMessagePromptVisibility(YES);
  // Start opening tabs.
  int numOfTabsThatWouldBeOpened = 10;
  // One NTP is already opened on startup.
  for (int i = 0; i < numOfTabsThatWouldBeOpened - 1; i++) {
    [ChromeEarlGrey openNewTab];
  }
  [ChromeEarlGreyUI openTabGrid];
  VerifyBottomMessagePromptVisibility(YES);
  // Close the last tab and check if the prompt is still visible.
  for (int i = numOfTabsThatWouldBeOpened - 1; i > 0; i--) {
    [ChromeEarlGrey closeTabAtIndex:i];
    VerifyBottomMessagePromptVisibility(YES);
  }
}

@end
