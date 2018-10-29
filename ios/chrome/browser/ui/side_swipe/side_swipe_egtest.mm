// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_view.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/testing/earl_grey/disabled_test_macros.h"
#include "net/test/embedded_test_server/default_handlers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Integration tests for side swipe.
@interface SideSwipeTestCase : ChromeTestCase
@end

@implementation SideSwipeTestCase

#pragma mark - Tests

// Tests that swiping horizontally on the bottom toolbar is changing tab.
- (void)testSideSwipeBottomToolbar {
  if (!IsSplitToolbarMode()) {
    EARL_GREY_TEST_SKIPPED(
        @"This tests should only be tested if the secondary toolbar is "
        @"present");
  }
  [self checkSideSwipeOnToolbarClass:[SecondaryToolbarView class]];
}

// Tests that swiping horizontally on the top toolbar is changing tab.
- (void)testSideSwipeTopToolbar {
  [self checkSideSwipeOnToolbarClass:[PrimaryToolbarView class]];
}

#pragma mark - Helpers

// Checks that side swipe on an element of class |klass| is working to change
// tab.
- (void)checkSideSwipeOnToolbarClass:(Class)klass {
  // Setup the server.
  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Load the first page.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebViewContainingText:"Echo"];

  // Open a new Tab to have a tab to switch to.
  [ChromeEarlGreyUI openNewTab];

  // Load the second page in the new tab.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/defaultresponse")];
  [ChromeEarlGrey waitForWebViewContainingText:"Default response"];

  // Side swipe on the toolbar.
  [[EarlGrey selectElementWithMatcher:grey_kindOfClass(klass)]
      performAction:grey_swipeSlowInDirection(kGREYDirectionRight)];

  // Check that we swiped back to our web page.
  [ChromeEarlGrey waitForWebViewContainingText:"Echo"];
}

@end
