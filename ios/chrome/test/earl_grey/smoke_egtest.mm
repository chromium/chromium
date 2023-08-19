// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

// Test case to verify that EarlGrey tests can be launched and perform basic UI
// interactions.
@interface SmokeTestCase : ChromeTestCase
@end

@implementation SmokeTestCase

// Tests that the tools menu is tappable.
- (void)testTapToolsMenu {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];
}

// Tests that a tab can be opened.
- (void)testOpenTab {
  [ChromeEarlGreyUI openNewTab];
  GREYAssertEqual(2, [ChromeEarlGrey mainTabCount], @"Expected 2 tabs.");
}
@end
