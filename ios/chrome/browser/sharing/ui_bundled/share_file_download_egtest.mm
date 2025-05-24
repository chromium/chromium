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

#pragma mark - Tests

// Tests that open in button appears when opening a PDF, and that tapping on it
// will open the activity view.
- (void)testOpenInPDF {
  // Open the activity menu.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPDFPath)];
  [self openActivityMenu];

  [ChromeEarlGrey verifyActivitySheetVisible];

  // Check that tapping on the Cancel button closes the activity menu and hides
  // the open in toolbar.
  [ChromeEarlGrey closeActivitySheet];
  [ChromeEarlGrey verifyActivitySheetNotVisible];
}

// Tests that open in button appears when opening a PNG, and that tapping on it
// will open the activity view.
- (void)testOpenInPNG {
  // Open the activity menu.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPNGPath)];
  [self openActivityMenu];
  [ChromeEarlGrey verifyActivitySheetVisible];

  // Check that tapping on the Cancel button closes the activity menu and hides
  // the open in toolbar.
  [ChromeEarlGrey closeActivitySheet];
  [ChromeEarlGrey verifyActivitySheetNotVisible];
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
  [ChromeEarlGrey verifyActivitySheetVisible];
  // Ensure that the link is shared.
  [ChromeEarlGrey verifyTextNotVisibleInActivitySheetWithID:kPNGFilename];
  [ChromeEarlGrey closeActivitySheet];
  [ChromeEarlGrey verifyActivitySheetNotVisible];
}

@end
