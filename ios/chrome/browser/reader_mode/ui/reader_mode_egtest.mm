// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/chrome/browser/popup_menu/ui_bundled/popup_menu_constants.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "testing/gmock/include/gmock/gmock-matchers.h"

// Tests interactions with Reader Mode on a web page.
@interface ReaderModeTestCase : ChromeTestCase
@end

@implementation ReaderModeTestCase

- (void)setUp {
  [super setUp];
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kEnableReaderMode);
  return config;
}

#pragma mark - Tests

// Tests that the user can show / hide Reader Mode from the tools menu
// entrypoint on an eligible web page.
- (void)testToggleReaderModeInToolsMenuForDistillablePage {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/article.html")];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Open Reader Mode UI.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuReaderMode)];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];

  // Close Reader Mode UI.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuReaderMode)];

  // The Reader Mode UI is not visible.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];
}

@end
