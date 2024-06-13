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
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

// Test suite that tests user interactions with the Bring Android Tabs
// confirmation alert modal.
@interface BringAndroidTabsConfirmationAlertTestCase : ChromeTestCase

@end

@implementation BringAndroidTabsConfirmationAlertTestCase

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
    SignInViaFREWithHistorySyncEnabled(YES);
  }
}

- (void)tearDown {
  CleanUp();
  [super tearDown];
}

// Tests that if the user relaunches the app without interacting with the
// confirmation alert modal, it would not be shown again.
- (void)testRestartWhenPromptIsVisible {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  [ChromeEarlGreyUI openTabGrid];
  VerifyConfirmationAlertPromptVisibility(YES);
  VerifyThatPromptDoesNotShowOnRestart(self.testServer->base_url());
}

// Tests that the user can open the list of recent Android tabs by tapping the
// "open" button on the confirmation alert modal. Afterwards, the modal would
// not be shown again after the user restarts.
- (void)testOpen {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  [ChromeEarlGreyUI openTabGrid];
  VerifyConfirmationAlertPromptVisibility(YES);
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConfirmationAlertPrimaryActionAccessibilityIdentifier)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForMainTabCount:GetTabCountOnPrompt() + /*new tab page*/ 1];
  [ChromeEarlGrey closeAllTabs];
  VerifyConfirmationAlertPromptVisibility(NO);
  VerifyThatPromptDoesNotShowOnRestart(self.testServer->base_url());
}

// Tests that the user can review the list of Android tabs by tapping the
// "review" button on the confirmation alert modal. Afterwards, the modal would
// not be shown again after the user restarts.
- (void)testReview {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  [ChromeEarlGreyUI openTabGrid];
  VerifyConfirmationAlertPromptVisibility(YES);
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConfirmationAlertTertiaryActionAccessibilityIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBringAndroidTabsPromptTabListAXId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  VerifyThatPromptDoesNotShowOnRestart(self.testServer->base_url());
}

// Tests that the user can close the confirmation alert modal by tapping the
// "close" button. The prompt would not be shown again when the user leaves the
// tab grid and comes back or restarts.
- (void)testTapCloseToDismiss {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  [ChromeEarlGreyUI openTabGrid];
  VerifyConfirmationAlertPromptVisibility(YES);
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConfirmationAlertSecondaryActionAccessibilityIdentifier)]
      performAction:grey_tap()];
  VerifyConfirmationAlertPromptVisibility(NO);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openTabGrid];
  VerifyConfirmationAlertPromptVisibility(NO);
  VerifyThatPromptDoesNotShowOnRestart(self.testServer->base_url());
}

// Tests that the user can swipe down the confirmation alert modal to dismiss
// it. The prompt would not be shown again when the user leaves the tab grid and
// comes back, or restarts.
- (void)testSwipeToDismiss {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  [ChromeEarlGreyUI openTabGrid];
  VerifyConfirmationAlertPromptVisibility(YES);
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBringAndroidTabsPromptConfirmationAlertAXId)]
      performAction:grey_swipeSlowInDirection(kGREYDirectionDown)];
  VerifyConfirmationAlertPromptVisibility(NO);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openTabGrid];
  VerifyConfirmationAlertPromptVisibility(NO);
  VerifyThatPromptDoesNotShowOnRestart(self.testServer->base_url());
}

@end
