// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/download/model/download_test_util.h"
#import "ios/chrome/browser/download/model/mime_type_util.h"
#import "ios/chrome/browser/download/ui_bundled/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::test::ios::kWaitForDownloadTimeout;
using chrome_test_util::ButtonWithAccessibilityLabelId;

namespace {

// Files landing page and download request handler.
std::unique_ptr<net::test_server::HttpResponse> GetResponse(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);

  if (request.GetURL().path() == "/") {
    result->set_content(
        "<a id='mobileconfig' href='/mobileconfig'>Mobileconfig</a>"
        "<br>"
        "<a id='calendar' href='/calendar'>Calendar</a>");
  } else if (request.GetURL().path() == "/mobileconfig") {
    result->AddCustomHeader("Content-Type", kMobileConfigurationType);
    result->set_content(
        testing::GetTestFileContents(testing::kMobileConfigFilePath));
  } else if (request.GetURL().path() == "/calendar") {
    result->AddCustomHeader("Content-Type", kCalendarMimeType);
    result->set_content(
        testing::GetTestFileContents(testing::kCalendarFilePath));
  }

  return result;
}

// Waits until the warning alert is shown.
[[nodiscard]] bool WaitForWarningAlert(NSString* alertMessage) {
  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        NSError* error = nil;
        [[EarlGrey selectElementWithMatcher:grey_text(alertMessage)]
            assertWithMatcher:grey_notNil()
                        error:&error];
        return (error == nil);
      });
}

}  // namespace

// Tests downloading files using SFSafariViewController.
@interface SafariDownloadEGTest : ChromeTestCase

@end

@implementation SafariDownloadEGTest

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  return config;
}

- (void)setUp {
  [super setUp];

  self.testServer->RegisterRequestHandler(base::BindRepeating(&GetResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that the correct warning alert is shown and when tapping 'Continue' a
// SFSafariViewController is presented.
- (void)testMobileConfigDownloadAndContinue {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Mobileconfig"];
  [ChromeEarlGrey tapWebStateElementWithID:@"mobileconfig"];

  GREYAssert(WaitForWarningAlert(l10n_util::GetNSString(
                 IDS_IOS_DOWNLOAD_MOBILECONFIG_FILE_WARNING_TITLE)),
             @"The warning alert did not show up");

  // Tap on 'Continue' to present the SFSafariViewController.
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_DOWNLOAD_MOBILECONFIG_CONTINUE))]
      performAction:grey_tap()];

  // Verify SFSafariViewController is presented.
  [[EarlGrey selectElementWithMatcher:grey_kindOfClassName(@"SFSafariView")]
      assertWithMatcher:grey_notNil()];
}

// Tests that a warning alert is shown and when tapping 'Cancel' the alert is
// dismissed without presenting a SFSafariViewController.
- (void)testMobileConfigDownloadAndCancel {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Mobileconfig"];
  [ChromeEarlGrey tapWebStateElementWithID:@"mobileconfig"];

  GREYAssert(WaitForWarningAlert(l10n_util::GetNSString(
                 IDS_IOS_DOWNLOAD_MOBILECONFIG_FILE_WARNING_TITLE)),
             @"The warning alert did not show up");

  // Tap on 'Cancel' to dismiss the warning alert.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(IDS_CANCEL)]
      performAction:grey_tap()];

  // Verify SFSafariViewController is not presented.
  [[EarlGrey selectElementWithMatcher:grey_kindOfClassName(@"SFSafariView")]
      assertWithMatcher:grey_nil()];
  // Verify the warning alert is dismissed.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_DOWNLOAD_MOBILECONFIG_FILE_WARNING_TITLE))]
      assertWithMatcher:grey_nil()];
}

// Tests that the correct warning alert is shown and when tapping 'Continue' a
// SFSafariViewController is presented.
- (void)testCalendarDownloadAndContinue {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Calendar"];
  [ChromeEarlGrey tapWebStateElementWithID:@"calendar"];

  GREYAssert(WaitForWarningAlert(l10n_util::GetNSString(
                 IDS_IOS_DOWNLOAD_CALENDAR_FILE_WARNING_MESSAGE)),
             @"The warning alert did not show up");
  // Tap on 'Continue' to present the SFSafariViewController.
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_DOWNLOAD_MOBILECONFIG_CONTINUE))]
      performAction:grey_tap()];

  // Verify SFSafariViewController is presented.
  [[EarlGrey selectElementWithMatcher:grey_kindOfClassName(@"SFSafariView")]
      assertWithMatcher:grey_notNil()];
}

// Tests that a warning alert is shown and when tapping 'Cancel' the alert is
// dismissed without presenting a SFSafariViewController.
- (void)testCalendarDownloadAndCancel {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Calendar"];
  [ChromeEarlGrey tapWebStateElementWithID:@"calendar"];

  GREYAssert(WaitForWarningAlert(l10n_util::GetNSString(
                 IDS_IOS_DOWNLOAD_CALENDAR_FILE_WARNING_MESSAGE)),
             @"The warning alert did not show up");

  // Tap on 'Cancel' to dismiss the warning alert.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(IDS_CANCEL)]
      performAction:grey_tap()];

  // Verify SFSafariViewController is not presented.
  [[EarlGrey selectElementWithMatcher:grey_kindOfClassName(@"SFSafariView")]
      assertWithMatcher:grey_nil()];
  // Verify the warning alert is dismissed.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_DOWNLOAD_MOBILECONFIG_FILE_WARNING_TITLE))]
      assertWithMatcher:grey_nil()];
}
@end
