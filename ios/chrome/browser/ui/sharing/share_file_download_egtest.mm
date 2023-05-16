// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* kPNGFilename = @"chromium_logo";

namespace {

// Path which leads to a PDF file.
const char kPDFPath[] = "/testpage.pdf";

// Path which leads to a PNG file.
const char kPNGPath[] = "/chromium_logo.png";

// Path which leads to a MOV file.
const char kMOVPath[] = "/video_sample.mov";

// Accessibility ID of the Activity menu.
NSString* kActivityMenuIdentifier = @"ActivityListView";

// Matcher for the Cancel button.
id<GREYMatcher> ShareMenuDismissButton() {
  return chrome_test_util::CloseButton();
}
}  // namespace

using base::test::ios::kWaitForDownloadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Tests Open in Feature.
@interface ShareFileDownloadTestCase : ChromeTestCase
@end

@implementation ShareFileDownloadTestCase

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

#pragma mark - Public

- (void)openActivityMenu {
  [ChromeEarlGreyUI openShareMenu];
}

- (void)closeActivityMenu {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // Tap the share button to dismiss the popover.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::TabShareButton()]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:ShareMenuDismissButton()]
        performAction:grey_tap()];
  }
}

- (void)assertActivityServiceVisible {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityID(kActivityMenuIdentifier)]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, condition),
             @"Waiting for the open in dialog to appear");
}

- (void)assertActivityMenuDismissed {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityID(kActivityMenuIdentifier)]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, condition),
             @"Waiting for the open in dialog to disappear");
}

#pragma mark - Tests

// Tests that open in button appears when opening a PDF, and that tapping on it
// will open the activity view.
- (void)testOpenInPDF {
  // Open the activity menu.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPDFPath)];
  [self openActivityMenu];

  [self assertActivityServiceVisible];

  // Check that tapping on the Cancel button closes the activity menu and hides
  // the open in toolbar.
  [self closeActivityMenu];
  [self assertActivityMenuDismissed];
}

// Tests that open in button appears when opening a PNG, and that tapping on it
// will open the activity view.
- (void)testOpenInPNG {
  // Open the activity menu.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPNGPath)];
  [self openActivityMenu];
  [self assertActivityServiceVisible];

  // Check that tapping on the Cancel button closes the activity menu and hides
  // the open in toolbar.
  [self closeActivityMenu];
  [self assertActivityMenuDismissed];
}

// Tests that open in button do not appears when opening a MOV file.
- (void)testOpenInMOV {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kMOVPath)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      assertWithMatcher:grey_nil()];
}

// Tests that open in button appears when opening a PNG and when shutting down
// the test server, the appropriate error message is displayed.
- (void)testOpenInOfflineServer {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPNGPath)];
  // Shutdown the test server.
  GREYAssertTrue(self.testServer->ShutdownAndWaitUntilComplete(),
                 @"Server did not shutdown.");

  // Open the activity menu.
  [self openActivityMenu];
  [self assertActivityServiceVisible];
  // Ensure that the link is shared.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(kPNGFilename),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [self closeActivityMenu];
  [self assertActivityMenuDismissed];
}

@end
