// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/chrome/browser/composebox/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/omnibox/public/omnibox_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

namespace {

// Matcher for the Composebox.
id<GREYMatcher> ComposeboxMatcher() {
  return grey_accessibilityID(kComposeboxAccessibilityIdentifier);
}

// Matcher for the clear button in the Composebox.
id<GREYMatcher> ComposeboxClearButtonMatcher() {
  return grey_allOf(
      grey_accessibilityID(kOmniboxClearButtonAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

}  // namespace

@interface ComposeboxTestCase : ChromeTestCase
@end

@implementation ComposeboxTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kComposeboxIOS);
  return config;
}

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that the Composebox is visible when tapping the omnibox.
- (void)testComposeboxVisibility {
  // Composebox is not available on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad as composebox is not available.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGreyUI focusOmnibox];

  // Clear the omnibox.
  [[EarlGrey selectElementWithMatcher:ComposeboxClearButtonMatcher()]
      performAction:grey_tap()];

  // Check for Composebox elements.
  // Plus button is visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxPlusButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Mic button is visible.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kComposeboxMicButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Send button is not visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxSendButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that typing in the Composebox shows the Send button.
- (void)testComposeboxSendButtonVisibility {
  // Composebox is not available on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad as composebox is not available.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGreyUI focusOmnibox];

  // Type some text.
  [[EarlGrey selectElementWithMatcher:ComposeboxMatcher()]
      performAction:grey_typeText(@"test")];

  // Send button is visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxSendButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Plus button is visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kComposeboxPlusButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Mic button is not visible.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kComposeboxMicButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

@end
