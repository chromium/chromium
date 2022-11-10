// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/open_in/open_in_controller_egtest.h"

#import <UIKit/UIKit.h>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/open_in/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_actions_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* kPNGFilename = @"chromium_logo";

namespace {

// Path which leads to a PDF file.
const char kPDFPath[] = "/testpage.pdf";

// Path wich leads to a PNG file.
const char kPNGPath[] = "/chromium_logo.png";

// Path wich leads to a MOV file.
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

@implementation ZZZOpenInManagerTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  config.additional_args.push_back(
      "--enable-features=" + std::string(kEnableOpenInDownload.name) + "<" +
      std::string(kEnableOpenInDownload.name));

  config.additional_args.push_back(
      "--force-fieldtrials=" + std::string(kEnableOpenInDownload.name) +
      "/Test");

  config.additional_args.push_back(
      "--force-fieldtrial-params=" + std::string(kEnableOpenInDownload.name) +
      ".Test:" + std::string(kOpenInDownloadParameterName) + "/" + _variant);

  return config;
}

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

#pragma mark - Public

- (void)openActivityMenu {
  // TODO(crbug.com/1357553): Remove when Open In download experiment is
  // finished.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      performAction:grey_tap()];
}

- (void)offlineAssertBehavior {
  // TODO(crbug.com/1357553): Remove when Open In download experiment is
  // finished.
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

- (BOOL)shouldSkipIpad {
  // Apple is hiding UIActivityViewController's content from the host app on
  // iPad.
  return YES;
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

- (BOOL)shouldRunTests {
  return YES;
}

#pragma mark - Tests

// Tests that open in button appears when opening a PDF, and that tapping on it
// will open the activity view.
- (void)testOpenInPDF {
  if (![self shouldRunTests]) {
    EARL_GREY_TEST_SKIPPED(@"Do not run the test.");
  }
  if ([ChromeEarlGrey isIPadIdiom] && [self shouldSkipIpad]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }
  // Open the activity menu.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPDFPath)];
  [self openActivityMenu];

  [self assertActivityServiceVisible];

  // Check that tapping on the Cancel button closes the activity menu and hides
  // the open in toolbar.
  [self closeActivityMenu];
  [self assertActivityMenuDismissed];
  // TODO(crbug.com/1357553): Remove when Open In download experiment is
  // finished.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that open in button appears when opening a PNG, and that tapping on it
// will open the activity view.
- (void)testOpenInPNG {
  if (![self shouldRunTests]) {
    EARL_GREY_TEST_SKIPPED(@"Do not run the test.");
  }
  if ([ChromeEarlGrey isIPadIdiom] && [self shouldSkipIpad]) {
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iPad.");
  }

  // Open the activity menu.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPNGPath)];
  [self openActivityMenu];
  [self assertActivityServiceVisible];

  // Check that tapping on the Cancel button closes the activity menu and hides
  // the open in toolbar.
  [self closeActivityMenu];
  [self assertActivityMenuDismissed];
  // TODO(crbug.com/1357553): Remove when Open In download experiment is
  // finished.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that open in button do not appears when opening a MOV file.
- (void)testOpenInMOV {
  if (![self shouldRunTests]) {
    EARL_GREY_TEST_SKIPPED(@"Do not run the test.");
  }
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kMOVPath)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      assertWithMatcher:grey_nil()];
}

// Tests that open in button appears when opening a PNG and when shutting down
// the test server, the appropriate error message is displayed.
- (void)testOpenInOfflineServer {
  if (![self shouldRunTests]) {
    EARL_GREY_TEST_SKIPPED(@"Do not run the test.");
  }
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPNGPath)];
  // Shutdown the test server.
  GREYAssertTrue(self.testServer->ShutdownAndWaitUntilComplete(),
                 @"Server did not shutdown.");

  // Open the activity menu.
  [self openActivityMenu];
  [self offlineAssertBehavior];
}

@end

// Test using WKDownload.
@interface OpenInWithWKDownloadTestCase : ZZZOpenInManagerTestCase
@end

@implementation OpenInWithWKDownloadTestCase

- (void)setUp {
  _variant = std::string(kOpenInDownloadWithWKDownloadParam);
  [super setUp];
}

- (void)openActivityMenu {
  [ChromeEarlGreyUI openShareMenu];
}

- (void)offlineAssertBehavior {
  [self assertActivityServiceVisible];
  // Ensure that the link is shared.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(kPNGFilename),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [self closeActivityMenu];
  [self assertActivityMenuDismissed];
}

- (BOOL)shouldSkipIpad {
  return NO;
}

- (BOOL)shouldRunTests {
  if (@available(iOS 14.5, *)) {
    return YES;
  }
  return NO;
}

// This is currently needed to prevent this test case from being ignored.
- (void)testEmpty {
}

@end

// Test using legacy download in share button.
@interface OpenInWithLegacyTestCase : ZZZOpenInManagerTestCase
@end

@implementation OpenInWithLegacyTestCase

- (void)setUp {
  _variant = std::string(kOpenInDownloadInShareButtonParam);
  [super setUp];
}

- (void)openActivityMenu {
  [ChromeEarlGreyUI openShareMenu];
}

- (void)offlineAssertBehavior {
  [self assertActivityServiceVisible];
  // Ensure that the link is shared.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(kPNGFilename),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [self closeActivityMenu];
  [self assertActivityMenuDismissed];
}

- (BOOL)shouldSkipIpad {
  return NO;
}

- (BOOL)shouldRunTests {
  if (@available(iOS 14.5, *)) {
    return YES;
  }
  return NO;
}

// This is currently needed to prevent this test case from being ignored.
- (void)testEmpty {
}

@end
