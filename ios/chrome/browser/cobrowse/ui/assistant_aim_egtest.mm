// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "components/omnibox/browser/aim_eligibility_service_features.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_ui_constants.h"
#import "ios/chrome/browser/composebox/eg_tests/composebox_app_interface.h"
#import "ios/chrome/browser/composebox/shared/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "url/gurl.h"

namespace {

// Waits for the assistant container to reach a specific detent.
void WaitForDetent(AssistantContainerDetent detent) {
  NSString* detentIdentifier;
  switch (detent) {
    case AssistantContainerDetent::kMinimized:
      detentIdentifier = kAssistantContainerDetentMinimizedIdentifier;
      break;
    case AssistantContainerDetent::kMedium:
      detentIdentifier = kAssistantContainerDetentMediumIdentifier;
      break;
    case AssistantContainerDetent::kLarge:
      detentIdentifier = kAssistantContainerDetentLargeIdentifier;
      break;
  }

  id<GREYMatcher> matcher = grey_accessibilityID(detentIdentifier);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:matcher];
}

// Opens the composebox, attaches the current tab, and waits for the send button
// to be enabled.
void OpenCoBrowse(net::EmbeddedTestServer* testServer) {
  [ComposeboxAppInterface setFuseboxEligible:YES];
  [ComposeboxAppInterface setTabUploadAutoSucceed:YES];

  [ChromeEarlGrey loadURL:testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Focus the omnibox. Tapping fake omnibox might not be enough on all pages.
  [ChromeEarlGreyUI focusOmnibox];

  // Wait for the composebox to be visible.
  id<GREYMatcher> composeboxMatcher =
      grey_accessibilityID(kComposeboxAccessibilityIdentifier);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:composeboxMatcher];

  // Tap on the plus button to open the menu.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxPlusButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Tap "Attach current tab" to trigger Co-browse.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kComposeboxAttachCurrentTabActionAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Wait for the send button to be enabled.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kComposeboxSendButtonAccessibilityIdentifier),
                     grey_enabled(), nil)];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxSendButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
}

}  // namespace

@interface AssistantAIMTestCase : ChromeTestCase
@end

@implementation AssistantAIMTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  // Enable features needed for composebox.
  config.features_enabled.push_back(kComposeboxIOS);
  config.features_enabled.push_back(kComposeboxIpad);
  config.features_enabled.push_back(kAimCobrowse);
  config.features_enabled.push_back(kAssistantContainer);
  config.features_disabled.push_back(kComposeboxAIMDisabled);
  config.features_disabled.push_back(omnibox::kAimServerEligibilityEnabled);
  config.features_disabled.push_back(kAssistantAimMinimizedState);
  return config;
}

- (void)setUp {
  [self addTeardownBlock:^{
    [ComposeboxAppInterface setAllToolsEnabled:NO];
    [ComposeboxAppInterface setFuseboxEligible:NO];
    [ComposeboxAppInterface setTabUploadAutoSucceed:NO];
  }];
  [super setUp];
  [ComposeboxAppInterface enableAllTools];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (void)testCloseButtonDismissesAssistant {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }
  OpenCoBrowse(self.testServer);

  // Wait for the assistant to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(
                          kAssistantAIMCloseButtonAccessibilityIdentifier)];

  // Tap the close button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kAssistantAIMCloseButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Verify the assistant is dismissed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kAssistantAIMCloseButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
}

- (void)testAssistantPersistsThroughTabGrid {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }
  OpenCoBrowse(self.testServer);

  // Wait for the assistant to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(
                          kAssistantAIMCloseButtonAccessibilityIdentifier)];
  // Enter Tab Grid.
  [ChromeEarlGreyUI openTabGrid];

  // Verify the assistant is NOT visible in Tab Grid.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kAssistantAIMCloseButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  // Exit Tab Grid.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];

  // Verify the assistant is visible again.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(
                          kAssistantAIMCloseButtonAccessibilityIdentifier)];
}
// Tests that the assistant can transition between medium, large, and minimized
// detents.
- (void)testDetentTransitions {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }
  OpenCoBrowse(self.testServer);

  // Wait for the assistant to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_accessibilityID(
                          kAssistantAIMCloseButtonAccessibilityIdentifier)];

  // Check it starts in Medium state.
  WaitForDetent(AssistantContainerDetent::kMedium);

  // Expand the assistant to Large state by swiping up.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kAssistantContainerDetentMediumIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  WaitForDetent(AssistantContainerDetent::kLarge);

  // Collapse the assistant to Minimized state by swiping down.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kAssistantContainerDetentLargeIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  WaitForDetent(AssistantContainerDetent::kMinimized);
}

// Tests that the assistant starts in minimized state when the flag is enabled.
// All 3 detents are available in this mode. This test verifies that we start
// in minimized and are not stuck in it.
- (void)testMinimizedStateWhenFlagEnabled {
  if ([ComposeboxAppInterface isServerSideStateEnabled]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped when kComposeboxServerSideState is enabled.");
  }
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  // Remove from disabled list to allow enabling it.
  std::erase(config.features_disabled, kAssistantAimMinimizedState);

  config.features_enabled.push_back(kAssistantAimMinimizedState);
  config.relaunch_policy = ForceRelaunchByKilling;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  OpenCoBrowse(self.testServer);

  // Wait for the assistant to appear and be visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kAssistantAIMCloseButtonAccessibilityIdentifier),
                     grey_sufficientlyVisible(), nil)];

  // Verify the assistant is in minimized state.
  WaitForDetent(AssistantContainerDetent::kMinimized);

  // Expand the assistant by swiping up.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kAssistantContainerDetentMinimizedIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Verify the assistant expanded.
  WaitForDetent(AssistantContainerDetent::kLarge);
}

@end
