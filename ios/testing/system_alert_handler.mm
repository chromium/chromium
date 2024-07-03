// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/system_alert_handler.h"

#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"

using base::test::ios::kWaitForUIElementTimeout;

namespace {

// Returns true if any static text child of `element` (XCUIElement*) contains
// `text` (NSString*).
BOOL ElementStaticTextContainsText(XCUIElement* element, NSString* text) {
  NSPredicate* label_contains_text =
      [NSPredicate predicateWithFormat:@"%K CONTAINS[c] %@", @"label", text];
  return
      [element.staticTexts containingPredicate:label_contains_text].count > 0;
}

// Closes the `alert` (XCUIElement*) if it matches a known text/button pair and
// returns whether it succeeded or not in closing it.
BOOL HandleSingleAlert(XCUIElement* alert) {
  NSDictionary<NSString*, NSArray<NSString*>*>* text_to_buttons =
      TextToButtonsOfKnownSystemAlerts();
  for (NSString* text in text_to_buttons) {
    if (ElementStaticTextContainsText(alert, text)) {
      NSLog(@"Found alert containing text: %@", text);

      for (NSString* button in text_to_buttons[text]) {
        if (!alert.buttons[button].exists) {
          NSLog(@"Button %@ doesn't exist. Skip tapping.", button);
          continue;
        }

        NSLog(@"Tapping alert button: %@", button);
        [alert.buttons[button] tap];
        return YES;
      }
    }
  }
  return NO;
}

}  // namespace

NSDictionary<NSString*, NSArray<NSString*>*>* TextToButtonsOfKnownSystemAlerts(
    void) {
  static NSDictionary<NSString*, NSArray<NSString*>*>* text_to_buttons = nil;
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    text_to_buttons = @{
      @"Software Update" : @[ @"Later", @"Remind Me Later" ],
      @"A new iOS update is now available." : @[ @"Close" ],
      @"Carrier Settings Update" : @[ @"Not Now" ],
      @"would like to find and connect to devices on your local network" :
          @[ @"OK", @"Allow" ],
      @"Unable to activate Touch ID on this iPhone." : @[ @"OK" ],
      @"Like to Access the Microphone" : @[ @"OK" ],
      @"Edit Home Screen" : @[ @"Dismiss" ],
      @"Apple ID Verification" : @[ @"Not Now" ],
      @"iPhone is not Activated" : @[ @"Dismiss" ],
      @"Apple Account Verification" : @[ @"Not Now" ],
      @"to find devices on local networks" : @[ @"Allow" ],
    };
  });
  return text_to_buttons;
}

BOOL HandleKnownSystemAlertsIfVisible(void) {
  XCUIApplication* springboard_app = [[XCUIApplication alloc]
      initWithBundleIdentifier:@"com.apple.springboard"];
  XCUIElement* alert = springboard_app.alerts.firstMatch;

  // Limit attempt times. If attempt limit is exceeded, it means something went
  // wrong in tapping the buttons.
  int attempt = 0;
  while (
      attempt < 5 &&
      [alert
          waitForExistenceWithTimeout:kWaitForUIElementTimeout.InSecondsF()]) {
    NSLog(@"Alert on screen: %@", alert.label);
    if (!HandleSingleAlert(alert)) {
      return NO;
    }
    attempt++;
  }
  return attempt < 5;
}
