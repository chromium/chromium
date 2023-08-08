// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <CoreGraphics/CoreGraphics.h>
#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/testing/system_alert_handler.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::kWaitForUIElementTimeout;

// Test suite to verify Internet connectivity.
@interface DeviceCheckTestCase : XCTestCase
@end

@implementation DeviceCheckTestCase

- (void)setUp {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  [app launch];
  XCTAssert(HandleKnownSystemAlertsIfVisible(), @"Unhandled system alert.");
}

// Verifies Internet connectivity by navigating to google.com.
- (void)testNetworkConnection {
  XCUIApplication* app = [[XCUIApplication alloc] init];

  struct CGVector offset;
  offset.dx = 0.5;
  offset.dy = 0.5;

  [[app.buttons[ntp_home::FakeOmniboxAccessibilityID()].firstMatch
      coordinateWithNormalizedOffset:offset] tap];

  XCTAssert(
      [app.keyboards.firstMatch
          waitForExistenceWithTimeout:kWaitForUIElementTimeout.InSecondsF()],
      @"Keyboard didn't appear!");
  [app typeText:@"http://google.com"];
  [app typeText:XCUIKeyboardKeyReturn];

  XCTAssert(
      // verify chrome is not showing offline dino page
      ![[[app.webViews.firstMatch descendantsMatchingType:XCUIElementTypeAny]
            matchingIdentifier:@"Dino game, play"]
              .firstMatch
          waitForExistenceWithTimeout:kWaitForPageLoadTimeout.InSecondsF()],
      @"Showing chrome dino page!");
}

@end
