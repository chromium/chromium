// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#import "base/test/ios/wait_util.h"
#include "ios/chrome/browser/download/download_test_util.h"
#include "ios/chrome/browser/download/usdz_mime_type.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if defined(CHROME_EARL_GREY_1)
#import <QuickLook/QuickLook.h>

// EG1 test relies on view controller presentation as the signal that Quick Look
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
using base::test::ios::kWaitForUIElementTimeout;

namespace {

// USDZ landing page and download request handler.
std::unique_ptr<net::test_server::HttpResponse> GetResponse(
    const net::test_server::HttpRequest& request) {
  auto result = std::make_unique<net::test_server::BasicHttpResponse>();
  result->set_code(net::HTTP_OK);

  if (request.GetURL().path() == "/") {
    result->set_content(
        "<a id='forbidden' href='/forbidden'>Forbidden</a> "
        "<a id='unauthorized' href='/unauthorized'>Unauthorized</a> "
        "<a id='changing-mime-type' href='/changing-mime-type'>Changing Mime "
        "Type</a> "
        "<a id='good' href='/good'>Good</a>");
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

  // QLPreviewController UI is rendered out of host process so EarlGrey matcher
  // can not find QLPreviewController UI.
#if defined(CHROME_EARL_GREY_1)
  // EG1 test relies on view controller presentation as the signal that
  // QLPreviewController UI is shown.
  id<BrowserInterface> interface =
      chrome_test_util::GetMainController().interfaceProvider.mainInterface;
  UIViewController* viewController = interface.viewController;
  bool shown = WaitUntilConditionOrTimeout(kWaitForDownloadTimeout, ^{
    UIViewController* presentedController =
        viewController.presentedViewController;
    return [presentedController class] == [QLPreviewController class];
  });
  GREYAssert(shown, @"QLPreviewController was not shown.");
#elif defined(CHROME_EARL_GREY_2)
  // EG2 test uses XCUIApplication API to check for Quick Look dialog UI
  // presentation.
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* goodTitle = app.staticTexts[@"good"];
  GREYAssert([goodTitle waitForExistenceWithTimeout:kWaitForDownloadTimeout],
             @"AR preview dialog UI was not presented");
#else
#error Must define either CHROME_EARL_GREY_1 or CHROME_EARL_GREY_2.
#endif
}

- (void)testDownloadUnauthorized {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Unauthorized"];
  [ChromeEarlGrey tapWebStateElementWithID:@"unauthorized"];

  // QLPreviewController UI is rendered out of host process so EarlGrey matcher
  // can not find QLPreviewController UI.
#if defined(CHROME_EARL_GREY_1)
  // EG1 test relies on view controller presentation as the signal that
  // QLPreviewController UI is shown.
  id<BrowserInterface> interface =
      chrome_test_util::GetMainController().interfaceProvider.mainInterface;
  UIViewController* viewController = interface.viewController;
  bool shown = WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    UIViewController* presentedController =
        viewController.presentedViewController;
    return [presentedController class] == [QLPreviewController class];
  });
  GREYAssertFalse(shown, @"QLPreviewController should not have shown.");
#elif defined(CHROME_EARL_GREY_2)
  // EG2 test uses XCUIApplication API to check for Quick Look dialog UI
  // presentation.
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* goodTitle = app.staticTexts[@"good"];
  GREYAssertFalse(
      [goodTitle waitForExistenceWithTimeout:kWaitForDownloadTimeout],
      @"AR preview dialog UI was presented");
#else
#error Must define either CHROME_EARL_GREY_1 or CHROME_EARL_GREY_2.
#endif
}

- (void)testDownloadForbidden {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Forbidden"];
  [ChromeEarlGrey tapWebStateElementWithID:@"forbidden"];

  // QLPreviewController UI is rendered out of host process so EarlGrey matcher
  // can not find QLPreviewController UI.
#if defined(CHROME_EARL_GREY_1)
  // EG1 test relies on view controller presentation as the signal that
  // QLPreviewController UI is shown.
  id<BrowserInterface> interface =
      chrome_test_util::GetMainController().interfaceProvider.mainInterface;
  UIViewController* viewController = interface.viewController;
  bool shown = WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    UIViewController* presentedController =
        viewController.presentedViewController;
    return [presentedController class] == [QLPreviewController class];
  });
  GREYAssertFalse(shown, @"QLPreviewController should not have shown.");
#elif defined(CHROME_EARL_GREY_2)
  // EG2 test uses XCUIApplication API to check for Quick Look dialog UI
  // presentation.
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* goodTitle = app.staticTexts[@"good"];
  GREYAssertFalse(
      [goodTitle waitForExistenceWithTimeout:kWaitForDownloadTimeout],
      @"AR preview dialog UI was presented");
#else
#error Must define either CHROME_EARL_GREY_1 or CHROME_EARL_GREY_2.
#endif
}

- (void)testDownloadChangingMimeType {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];
  [ChromeEarlGrey waitForWebStateContainingText:"Changing Mime Type"];
  [ChromeEarlGrey tapWebStateElementWithID:@"changing-mime-type"];

  // QLPreviewController UI is rendered out of host process so EarlGrey matcher
  // can not find QLPreviewController UI.
#if defined(CHROME_EARL_GREY_1)
  // EG1 test relies on view controller presentation as the signal that
  // QLPreviewController UI is shown.
  id<BrowserInterface> interface =
      chrome_test_util::GetMainController().interfaceProvider.mainInterface;
  UIViewController* viewController = interface.viewController;
  bool shown = WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    UIViewController* presentedController =
        viewController.presentedViewController;
    return [presentedController class] == [QLPreviewController class];
  });
  GREYAssertFalse(shown, @"QLPreviewController should not have shown.");
#elif defined(CHROME_EARL_GREY_2)
  // EG2 test uses XCUIApplication API to check for Quick Look dialog UI
  // presentation.
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* goodTitle = app.staticTexts[@"good"];
  GREYAssertFalse(
      [goodTitle waitForExistenceWithTimeout:kWaitForDownloadTimeout],
      @"AR preview dialog UI was presented");
#else
#error Must define either CHROME_EARL_GREY_1 or CHROME_EARL_GREY_2.
#endif
}

@end
