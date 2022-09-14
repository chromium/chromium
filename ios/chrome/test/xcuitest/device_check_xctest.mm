// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {
// TODO(crbug.com/1112912): Share the logic and constants with
// ios/testing/earl_grey/base_earl_grey_test_case.mm.
// Alert labels (partial match) and corresponding buttons (exact match) to
// dismiss the alert.
NSArray<NSArray<NSString*>*>* alert_text_button_pairs = @[
  @[ @"Software Update", @"Later" ],
  @[ @"Software Update", @"Remind Me Later" ],
  @[ @"A new iOS update is now available.", @"Close" ],
  @[ @"Carrier Settings Update", @"Not Now" ],
  @[
    @"would like to find and connect to devices on your local network.", @"OK"
  ],
  @[ @"Unable to activate Touch ID on this iPhone.", @"OK" ],
  @[ @"Like to Access the Microphone", @"OK" ],
];

BOOL ElementStaticTextContainsText(XCUIElement* element, NSString* text) {
  NSPredicate* label_contains_text =
      [NSPredicate predicateWithFormat:@"%K CONTAINS[c] %@", @"label", text];
  return
      [element.staticTexts containingPredicate:label_contains_text].count > 0;
}

BOOL HandleSingleAlert(XCUIElement* alert) {
  for (NSArray<NSString*>* pair : alert_text_button_pairs) {
    if (ElementStaticTextContainsText(alert, pair[0])) {
      NSLog(@"Found alert containing text: %@", pair[0]);
      if (!alert.buttons[pair[1]].exists) {
        NSLog(@"Button %@ doesn't exist. Skip tapping.", pair[1]);
        continue;
      }
      NSLog(@"Tapping alert button: %@", pair[1]);
      [alert.buttons[pair[1]] tap];
      return YES;
    }
  }
  return NO;
}

BOOL HandleSystemAlertsIfVisible() {
  XCUIApplication* springboard_app = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.springboard"];
  XCUIElement* alert =
      [springboard_app descendantsMatchingType:XCUIElementTypeAlert].firstMatch;

  // Limit attempt times. If attempt limit is exceeded it means something wrong
  // at tapping the dismiss button.
  int attempt = 0;
  while (attempt < 5 &&
         [alert waitForExistenceWithTimeout:kWaitForUIElementTimeout]) {
    NSLog(@"Alert on screen: %@", alert.label);
    if (!HandleSingleAlert(alert)) {
      return NO;
    }
    attempt++;
  }
  return attempt < 5;
}
}  // namespace

// Test suite to verify Internet connectivity.
@interface DeviceCheckTestCase : XCTestCase
@end

@implementation DeviceCheckTestCase

- (void)setUp {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  [app launch];
  XCTAssert(HandleSystemAlertsIfVisible(), @"Unhandled system alert.");
}

// Verifies Internet connectivity by navigating to google.com.
- (void)testNetworkConnection {
  XCUIApplication* app = [[XCUIApplication alloc] init];

  [app.buttons[ntp_home::FakeOmniboxAccessibilityID()].firstMatch tap];

  XCTAssert([app.keyboards.firstMatch
                waitForExistenceWithTimeout:kWaitForUIElementTimeout],
            @"Keyboard didn't appear!");
  [app typeText:@"http://google.com"];
  [app typeText:XCUIKeyboardKeyReturn];

  XCTAssert(
      // verify chrome is not showing offline dino page
      ![[[app.webViews.firstMatch descendantsMatchingType:XCUIElementTypeAny]
            matchingIdentifier:@"Dino game, play"]
              .firstMatch waitForExistenceWithTimeout:kWaitForPageLoadTimeout],
      @"Showing chrome dino page!");
}

@end
