// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#import "base/test/ios/wait_util.h"
#include "ios/chrome/browser/download/download_test_util.h"
#include "ios/chrome/browser/download/mime_type_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/l10n/l10n_util_mac.h"


#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForDownloadTimeout;
using base::test::ios::kWaitForUIElementTimeout;

namespace {

// Use separate timeout for EG2 tests to accomodate for IPC delays.
const NSTimeInterval kWaitForARPresentationTimeout = 30.0;

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
#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  // TODO(crbug.com/1114202): The XCUIElement queries in this test are broken on
  // Xcode 12 beta 4 when running on the iOS 12 simulator.  Disable until Xcode
  // is fixed.
  if (@available(iOS 13, *)) {
  } else {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS12.");
  }
#endif

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Good"];
  [ChromeEarlGrey tapWebStateElementWithID:@"good"];

  // QLPreviewController UI is rendered out of host process so EarlGrey matcher
  // can not find QLPreviewController UI.
  // EG2 test uses XCUIApplication API to check for Quick Look dialog UI
  // presentation.
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* goodTitle = app.staticTexts[@"good"];
#if TARGET_IPHONE_SIMULATOR
  if (@available(iOS 14, *)) {
    goodTitle = app.staticTexts[@"Unsupported file format"];
  }
#endif
  GREYAssert(
      [goodTitle waitForExistenceWithTimeout:kWaitForARPresentationTimeout],
      @"AR preview dialog UI was not presented");
}

- (void)testDownloadUnauthorized {
#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  // TODO(crbug.com/1114202): The XCUIElement queries in this test are broken on
  // Xcode 12 beta 4 when running on the iOS 12 simulator.  Disable until Xcode
  // is fixed.
  if (@available(iOS 13, *)) {
  } else {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS12.");
  }
#endif

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Unauthorized"];
  [ChromeEarlGrey tapWebStateElementWithID:@"unauthorized"];

  // QLPreviewController UI is rendered out of host process so EarlGrey matcher
  // can not find QLPreviewController UI.
  // EG2 test uses XCUIApplication API to check for Quick Look dialog UI
  // presentation.
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* goodTitle = app.staticTexts[@"good"];
#if TARGET_IPHONE_SIMULATOR
  if (@available(iOS 14, *)) {
    goodTitle = app.staticTexts[@"Unsupported file format"];
  }
#endif
  GREYAssertFalse(
      [goodTitle waitForExistenceWithTimeout:kWaitForARPresentationTimeout],
      @"AR preview dialog UI was presented");
}

- (void)testDownloadForbidden {
#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  // TODO(crbug.com/1114202): The XCUIElement queries in this test are broken on
  // Xcode 12 beta 4 when running on the iOS 12 simulator.  Disable until Xcode
  // is fixed.
  if (@available(iOS 13, *)) {
  } else {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS12.");
  }
#endif

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Forbidden"];
  [ChromeEarlGrey tapWebStateElementWithID:@"forbidden"];

  // QLPreviewController UI is rendered out of host process so EarlGrey matcher
  // can not find QLPreviewController UI.
  // EG2 test uses XCUIApplication API to check for Quick Look dialog UI
  // presentation.
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* goodTitle = app.staticTexts[@"good"];
#if TARGET_IPHONE_SIMULATOR
  if (@available(iOS 14, *)) {
    goodTitle = app.staticTexts[@"Unsupported file format"];
  }
#endif
  GREYAssertFalse(
      [goodTitle waitForExistenceWithTimeout:kWaitForARPresentationTimeout],
      @"AR preview dialog UI was presented");
}

- (void)testDownloadChangingMimeType {
#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  // TODO(crbug.com/1114202): The XCUIElement queries in this test are broken on
  // Xcode 12 beta 4 when running on the iOS 12 simulator.  Disable until Xcode
  // is fixed.
  if (@available(iOS 13, *)) {
  } else {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS12.");
  }
#endif

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Changing Mime Type"];
  [ChromeEarlGrey tapWebStateElementWithID:@"changing-mime-type"];

  // QLPreviewController UI is rendered out of host process so EarlGrey matcher
  // can not find QLPreviewController UI.
  // EG2 test uses XCUIApplication API to check for Quick Look dialog UI
  // presentation.
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* goodTitle = app.staticTexts[@"good"];
#if TARGET_IPHONE_SIMULATOR
  if (@available(iOS 14, *)) {
    goodTitle = app.staticTexts[@"Unsupported file format"];
  }
#endif
  GREYAssertFalse(
      [goodTitle waitForExistenceWithTimeout:kWaitForARPresentationTimeout],
      @"AR preview dialog UI was presented");
}

// Tests that the visibilitychange event is fired when quicklook is
// shown/hidden.
- (void)testVisibilitychangeEventFired {
#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  // TODO(crbug.com/1114202): The XCUIElement queries in this test are broken on
  // Xcode 12 beta 4 when running on the iOS 12 simulator.  Disable until Xcode
  // is fixed.
  if (@available(iOS 13, *)) {
  } else {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS12.");
  }
#endif

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Good"];
  [ChromeEarlGrey tapWebStateElementWithID:@"good"];

  [ChromeEarlGrey waitForWebStateContainingText:"hidden"];

  // QLPreviewController UI is rendered out of host process so EarlGrey matcher
  // can not find QLPreviewController UI.
  // EG2 test uses XCUIApplication API to check for Quick Look dialog UI
  // presentation.
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* goodTitle = app.staticTexts[@"good"];
#if TARGET_IPHONE_SIMULATOR
  if (@available(iOS 14, *)) {
    goodTitle = app.staticTexts[@"Unsupported file format"];
  }
#endif
  GREYAssert(
      [goodTitle waitForExistenceWithTimeout:kWaitForARPresentationTimeout],
      @"AR preview dialog UI was not presented");

  // Close the QuickLook dialog.
  XCUIElement* doneButton = app.buttons[@"Done"];
  GREYAssert(
      [doneButton waitForExistenceWithTimeout:kWaitForARPresentationTimeout],
      @"Done button not visible");
  [doneButton tap];

  // Check that the visibilitychange event is triggered.
  [ChromeEarlGrey waitForWebStateContainingText:"visible"];
}

@end
