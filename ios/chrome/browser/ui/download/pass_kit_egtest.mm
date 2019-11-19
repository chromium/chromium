// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <PassKit/PassKit.h>

#include <memory>

#include "base/bind.h"
#import "base/test/ios/wait_util.h"
#include "ios/chrome/browser/download/download_test_util.h"
#include "ios/chrome/browser/download/pass_kit_mime_type.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if defined(CHROME_EARL_GREY_1)
// EG1 test relies on view controller presentation as the signal that PassKit
// Dialog is shown.
#import "ios/chrome/app/main_controller.h"  // nogncheck
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"  // nogncheck
#import "ios/chrome/test/app/chrome_test_util.h"  // nogncheck
#endif

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForDownloadTimeout;

namespace {

// Returns matcher for PassKit error infobar.
id<GREYMatcher> PassKitErrorInfobar() {
  using l10n_util::GetNSStringWithFixup;
  NSString* label = GetNSStringWithFixup(IDS_IOS_GENERIC_PASSKIT_ERROR);
  return grey_accessibilityLabel(label);
}

// PassKit landing page and download request handler.
std::unique_ptr<net::test_server::HttpResponse> GetResponse(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);

  if (request.GetURL().path() == "/") {
    result->set_content(
        "<a id='bad' href='/bad'>Bad</a>"
        "<a id='good' href='/good'>Good</a>");
  } else if (request.GetURL().path() == "/bad") {
    result->AddCustomHeader("Content-Type", kPkPassMimeType);
    result->set_content("corrupted");
  } else if (request.GetURL().path() == "/good") {
    result->AddCustomHeader("Content-Type", kPkPassMimeType);
    result->set_content(testing::GetTestFileContents(testing::kPkPassFilePath));
  }

  return result;
}

}  // namespace

// Tests PassKit file download.
@interface PassKitEGTest : ChromeTestCase
@end

@implementation PassKitEGTest

- (void)setUp {
  [super setUp];

  self.testServer->RegisterRequestHandler(base::Bind(&GetResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that Chrome presents PassKit error infobar if pkpass file cannot be
// parsed.
- (void)testPassKitParsingError {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Bad"];
  [ChromeEarlGrey tapWebStateElementWithID:@"bad"];

  bool infobarShown = WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:PassKitErrorInfobar()]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return (error == nil);
  });
  GREYAssert(infobarShown, @"PassKit error infobar was not shown");
}

// Tests that Chrome PassKit dialog is shown for sucessfully downloaded pkpass
// file.
- (void)testPassKitDownload {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Wallet app is not supported on iPads.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Good"];
  [ChromeEarlGrey tapWebStateElementWithID:@"good"];

  // PKAddPassesViewController UI is rendered out of host process so EarlGrey
  // matcher can not find PassKit Dialog UI.
#if defined(CHROME_EARL_GREY_1)
  // EG1 test relies on view controller presentation as the signal that PassKit
  // Dialog is shown.
  id<BrowserInterface> interface =
      chrome_test_util::GetMainController().interfaceProvider.mainInterface;
  UIViewController* viewController = interface.viewController;
  bool dialogShown = WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    UIViewController* presentedController =
        viewController.presentedViewController;
    return [presentedController class] == [PKAddPassesViewController class];
  });
  GREYAssert(dialogShown, @"PassKit dialog was not shown");
#elif defined(CHROME_EARL_GREY_2)
  // EG2 test can use XCUIApplication API to check for PassKit dialog UI
  // presentation.
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* title = nil;
  if (@available(iOS 13, *)) {
    title = app.staticTexts[@"Toy Town Membership"];
  } else {
    title = app.otherElements[@"Toy Town Membership"];
  }
  GREYAssert([title waitForExistenceWithTimeout:kWaitForDownloadTimeout],
             @"PassKit dialog UI was not presented");
#else
#error Must define either CHROME_EARL_GREY_1 or CHROME_EARL_GREY_2.
#endif
}

@end
