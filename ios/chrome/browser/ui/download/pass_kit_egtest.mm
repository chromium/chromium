// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <PassKit/PassKit.h>

#include <memory>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/app/main_controller.h"
#include "ios/chrome/browser/download/pass_kit_mime_type.h"
#include "ios/chrome/browser/download/pass_kit_test_util.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForDownloadTimeout;
using chrome_test_util::GetMainController;

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
    result->set_content(testing::GetTestPass());
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
  [ChromeEarlGrey waitForWebViewContainingText:"Bad"];
  [ChromeEarlGrey tapWebViewElementWithID:@"bad"];

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
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Wallet app is not supported on iPads.");
  }

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebViewContainingText:"Good"];
  [ChromeEarlGrey tapWebViewElementWithID:@"good"];

  // PKAddPassesViewController UI is rendered out of host process so EarlGrey
  // matcher can not find PassKit Dialog UI. Instead this test relies on view
  // controller presentation as the signal that PassKit Dialog is shown.
  UIViewController* BVC = GetMainController().browserViewInformation.mainBVC;
  bool dialogShown = WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    UIViewController* presentedController = BVC.presentedViewController;
    return [presentedController class] == [PKAddPassesViewController class];
  });
  GREYAssert(dialogShown, @"PassKit dialog was not shown");
}

@end
