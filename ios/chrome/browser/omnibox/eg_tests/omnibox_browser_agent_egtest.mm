// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/eg_tests/omnibox_app_interface.h"
#import "ios/chrome/browser/omnibox/eg_tests/omnibox_earl_grey.h"
#import "ios/chrome/browser/omnibox/eg_tests/omnibox_matchers.h"
#import "ios/chrome/browser/omnibox/eg_tests/omnibox_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

// Tests the omnibox browser agent.
@interface OmniboxBrowserAgentTestCase : ChromeTestCase
@end

@implementation OmniboxBrowserAgentTestCase

- (void)setUp {
  [super setUp];

  // Start a server to be able to navigate to a web page.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&omnibox::OmniboxHTTPResponses));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

#pragma mark - Helpers

- (void)expectIsOmniboxFocused:(BOOL)expectFocused {
  if (expectFocused) {
    GREYAssertTrue([OmniboxAppInterface isOmniboxFocusedOnMainBrowser],
                   @"IsOmniboxFocused is expected to be true.");
  } else {
    GREYAssertFalse([OmniboxAppInterface isOmniboxFocusedOnMainBrowser],
                    @"IsOmniboxFocused is expected to be false.");
  }
}

// Taps the fake omnibox and waits for the real omnibox to be visible.
- (void)focusFakebox {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
}

#pragma mark - Tests

// Tests that IsOmniboxFocused returns the expected valued when
// focusing/defocusing the omnibox.
- (void)testIsOmniboxFocused {
  // Open a web page.
  [OmniboxEarlGrey openPage:omnibox::Page(1) testServer:self.testServer];
  [self expectIsOmniboxFocused:NO];

  // Focus the omnibox.
  [ChromeEarlGreyUI focusOmnibox];
  [self expectIsOmniboxFocused:YES];

  // Tap the clear button. This hides the popup.
  [[EarlGrey selectElementWithMatcher:omnibox::ClearButtonMatcher()]
      performAction:grey_tap()];
  [self expectIsOmniboxFocused:YES];

  // Defocus the omnibox.
  [OmniboxEarlGrey defocusOmnibox];
  [self expectIsOmniboxFocused:NO];

  // Open NTP.
  [ChromeEarlGrey openNewTab];
  [self expectIsOmniboxFocused:NO];

  // Focus the fakebox.
  [self focusFakebox];
  [self expectIsOmniboxFocused:YES];

  // Defocus the omnibox.
  [OmniboxEarlGrey defocusOmnibox];
  [self expectIsOmniboxFocused:NO];
}

@end
