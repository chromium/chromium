// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/download/model/download_test_util.h"
#import "ios/chrome/browser/download/model/mime_type_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/http/http_status_code.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForDownloadTimeout;
using base::test::ios::kWaitForUIElementTimeout;

namespace {

// USDZ landing page and download request handler.
std::unique_ptr<net::test_server::HttpResponse> GetResponse(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);

  if (request.GetURL().path() == "/") {
    result->set_content(
        "<html><head><script>"
        "document.addEventListener('visibilitychange', "
        "function() {"
        "document.getElementById('visibility-change').innerHTML = "
        "document.visibilityState;"
        "});"
        "</script></head><body>"
        "<a id='forbidden' href='/forbidden'>Forbidden</a> "
        "<a id='unauthorized' href='/unauthorized'>Unauthorized</a> "
        "<a id='changing-mime-type' href='/changing-mime-type'>Changing Mime "
        "Type</a> "
        "<a id='good' href='/good'>Good</a>"
        "<p id='visibility-change'>None</p>"
        "</body></html>");
    return result;
  }

  if (request.GetURL().path() == "/forbidden") {
    result->set_code(net::HTTP_FORBIDDEN);
  } else if (request.GetURL().path() == "/unauthorized") {
    result->set_code(net::HTTP_UNAUTHORIZED);
  } else if (request.GetURL().path() == "/changing-mime-type") {
    result->set_code(net::HTTP_OK);
    result->AddCustomHeader("Content-Type", "unknown");
    result->set_content(testing::GetTestFileContents(testing::kUsdzFilePath));
  } else if (request.GetURL().path() == "/good") {
    result->set_code(net::HTTP_OK);
    result->AddCustomHeader("Content-Type", kUsdzMimeType);
    result->set_content(testing::GetTestFileContents(testing::kUsdzFilePath));
  }

  return result;
}

}  // namespace

// Tests previewing USDZ format files.
@interface ARQuickLookEGTest : ChromeTestCase

@end

@implementation ARQuickLookEGTest

- (void)setUp {
  [super setUp];

  self.testServer->RegisterRequestHandler(base::BindRepeating(&GetResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that QLPreviewController is shown for sucessfully downloaded USDZ file.
- (void)testDownloadUsdz {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Good"];
  [ChromeEarlGrey tapWebStateElementWithID:@"good"];

  // Verify QLPreviewControllerView is presented.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      grey_kindOfClassName(@"QLPreviewControllerView")];
}

- (void)testDownloadUnauthorized {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Unauthorized"];
  [ChromeEarlGrey tapWebStateElementWithID:@"unauthorized"];

  // Verify QLPreviewControllerView is not presented.
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClassName(@"QLPreviewControllerView")]
      assertWithMatcher:grey_nil()];
}

- (void)testDownloadForbidden {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Forbidden"];
  [ChromeEarlGrey tapWebStateElementWithID:@"forbidden"];

  // Verify QLPreviewControllerView is not presented.
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClassName(@"QLPreviewControllerView")]
      assertWithMatcher:grey_nil()];
}

- (void)testDownloadChangingMimeType {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Changing Mime Type"];
  [ChromeEarlGrey tapWebStateElementWithID:@"changing-mime-type"];

  // Verify QLPreviewControllerView is not presented.
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClassName(@"QLPreviewControllerView")]
      assertWithMatcher:grey_nil()];
}

@end
