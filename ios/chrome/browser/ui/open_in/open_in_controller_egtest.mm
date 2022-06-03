// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Path which leads to a PDF file.
const char kPDFPath[] = "/testpage.pdf";

// Path wich leads to a PNG file.
const char kPNGPath[] = "/chromium_logo.png";

// Path wich leads to a MOV file.
const char kMOVPath[] = "/video_sample.mov";

// Matcher for the Cancel button.
id<GREYMatcher> ShareMenuDismissButton() {
  return chrome_test_util::CloseButton();
}

}  // namespace

using base::test::ios::kWaitForDownloadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Tests Open in Feature.
@interface OpenInManagerTestCase : ChromeTestCase
@end

@implementation OpenInManagerTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  return config;
}

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

// Tests that open in button appears when opening a PDF, and that tapping on it
// will open the activity view.
- (void)testOpenInPDF {
  // Apple is hiding UIActivityViewController's content from the host app on
  // iPad.
  if ([ChromeEarlGrey isIPadIdiom])
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");

  // Open the activity menu.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPDFPath)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      performAction:grey_tap()];

  // Wait for the dialog with filename label to appear.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_text(@"testpage"),
                                            grey_sufficientlyVisible(), nil)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, condition),
             @"Waiting for the open in dialog to appear");

  // Check that tapping on the Cancel button closes the activity menu and hides
  // the open in toolbar.
  [[EarlGrey selectElementWithMatcher:ShareMenuDismissButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ShareMenuDismissButton()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that open in button appears when opening a PNG, and that tapping on it
// will open the activity view.
- (void)testOpenInPNG {
  // Apple is hiding UIActivityViewController's content from the host app on
  // iPad.
  if ([ChromeEarlGrey isIPadIdiom])
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");

  // Open the activity menu.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPNGPath)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      performAction:grey_tap()];

  // Wait for the dialog with filename label to appear.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_text(@"chromium_logo"),
                                            grey_sufficientlyVisible(), nil)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, condition),
             @"Waiting for the open in dialog to appear");

  // Check that tapping on the Cancel button closes the activity menu and hides
  // the open in toolbar.
  [[EarlGrey selectElementWithMatcher:ShareMenuDismissButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ShareMenuDismissButton()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      assertWithMatcher:grey_notVisible()];
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
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      assertWithMatcher:grey_notNil()];

  // Shutdown the test server.
  GREYAssertTrue(self.testServer->ShutdownAndWaitUntilComplete(),
                 @"Server did not shutdown.");

  // Open the activity menu.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      performAction:grey_tap()];

  // Wait for the dialog containing the error to appear.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_text(l10n_util::GetNSStringWithFixup(
                                         IDS_IOS_OPEN_IN_FILE_DOWNLOAD_FAILED)),
                                     grey_sufficientlyVisible(), nil)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, condition),
             @"Waiting for the error dialog to appear");
}

@end
