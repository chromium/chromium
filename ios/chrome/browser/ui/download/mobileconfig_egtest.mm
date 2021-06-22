// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/download/download_test_util.h"
#include "ios/chrome/browser/download/mime_type_util.h"
#import "ios/chrome/browser/ui/download/features.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForDownloadTimeout;
using chrome_test_util::ButtonWithAccessibilityLabelId;

namespace {

// .mobileconfig file landing page and download request handler.
std::unique_ptr<net::test_server::HttpResponse> GetResponse(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);

  if (request.GetURL().path() == "/") {
    result->set_content("<a id='download' href='/download'>Download</a>");
  } else {
    result->AddCustomHeader("Content-Type", kMobileConfigurationType);
    result->set_content(
        testing::GetTestFileContents(testing::kMobileConfigFilePath));
  }

  return result;
}

// Waits until the warning alert is shown.
bool WaitForWarningAlert() WARN_UNUSED_RESULT;
bool WaitForWarningAlert() {
  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        NSError* error = nil;
        [[EarlGrey selectElementWithMatcher:
                       grey_text(l10n_util::GetNSString(
                           IDS_IOS_DOWNLOAD_MOBILECONFIG_FILE_WARNING_TITLE))]
            assertWithMatcher:grey_notNil()
                        error:&error];
        return (error == nil);
      });
}

}  // namespace

// Tests MobileConfigEGTest file download.
@interface MobileConfigEGTest : ChromeTestCase

@end

@implementation MobileConfigEGTest

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kDownloadMobileConfigFile);
  return config;
}

- (void)setUp {
  [super setUp];

  self.testServer->RegisterRequestHandler(base::BindRepeating(&GetResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that a warning alert is shown and when tapping 'Continue' a
// SFSafariViewController is presented.
- (void)testMobileConfigDownloadAndContinue {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];

  GREYAssert(WaitForWarningAlert(), @"The warning alert did not show up");

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
  [ChromeEarlGrey waitForWebStateContainingText:"Download"];
  [ChromeEarlGrey tapWebStateElementWithID:@"download"];

  GREYAssert(WaitForWarningAlert(), @"The warning alert did not show up");

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
