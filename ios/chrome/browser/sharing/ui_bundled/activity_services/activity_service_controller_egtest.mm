// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import <memory>

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/popup_menu/public/popup_menu_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_test_case_app_interface.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// The text of the Share Sheet item for the Open Extension.
NSString* const kEGOpenExtension = @"EGOpenExtension";

const char kPotatoPath[] = "/potato";

// A request handler that provides the "tomato" response.
std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  if (request.relative_url == kPotatoPath) {
    response->set_code(net::HTTP_OK);
    response->set_content("tomato");
    response->set_content_type("text/plain");
  } else {
    response->set_code(net::HTTP_NOT_FOUND);
  }
  return response;
}
}  // namespace

// Earl grey integration tests for Activity Service Controller.
@interface ActivityServiceControllerTestCase : ChromeTestCase
@end

@implementation ActivityServiceControllerTestCase

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(base::BindRepeating(&HandleRequest));
  GREYAssertTrue(self.testServer->Start(),
                 @"EmbeddedTestServer failed to start.");
}

- (void)testOpenActivityServiceControllerAndCopy {
  GURL url = self.testServer->GetURL(kPotatoPath);

  // Open page and open the share menu.
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGreyUI openShareMenu];

  // Verify that the share menu is up and contains a Copy action.
  [ChromeEarlGrey verifyActivitySheetVisible];
  // Start the Copy action and verify that the share menu gets dismissed.
  [ChromeEarlGrey tapButtonInActivitySheetWithID:@"Copy"];
  [ChromeEarlGrey verifyActivitySheetNotVisible];
}

// Tests that the open extension opens a new tab.
// TODO(crbug.com/484191734) Test is failing
- (void)DISABLED_testOpenActivityServiceControllerAndOpenExtension {
  GURL url = self.testServer->GetURL(kPotatoPath);

  // Open page and open the share menu.
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGreyUI openShareMenu];

  [ChromeEarlGrey verifyActivitySheetVisible];

  if (@available(iOS 26.0, *)) {
    [ChromeEarlGrey tapMoreOptionButtonInActivitySheet];

    // TODO(crbug.com/432223861): Revisit using
    // `tapButtonInActivitySheetWithID` if it can be fixed to work better on
    // iOS 26, or if Apple fixes a bug that caused the item to not be
    // hittable.
    [ChromeEarlGrey verifyTextVisibleInActivitySheetWithID:kEGOpenExtension];
    XCUIApplication* app = [[XCUIApplication alloc] init];
    XCUIElement* button = app.otherElements[@"ActivityListView"]
                              .staticTexts[kEGOpenExtension]
                              .firstMatch;
    // Tap the coordinates of the center of the button instead of calling
    // `[button tap]`. This avoids an issue where calling `tap` on the button
    // might cause it to try to scroll to the button and inadvertently cause
    // the button in question to become not hittable.
    XCUICoordinate* buttonCenter =
        [button coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.5)];
    [buttonCenter tap];
  } else {
    [ChromeEarlGrey tapButtonInActivitySheetWithID:kEGOpenExtension];
  }

  GREYCondition* tabCountCheck =
      [GREYCondition conditionWithName:@"Tab count"
                                 block:^{
                                   return [ChromeEarlGrey mainTabCount] == 2;
                                 }];
  if (![tabCountCheck waitWithTimeout:base::test::ios::kWaitForUIElementTimeout
                                          .InSecondsF()]) {
    // If the tab is not opened, it is very likely due to a system popup.
    // Try to find it and open on the "Open" button.
    XCUIApplication* springboardApplication = [[XCUIApplication alloc]
        initWithBundleIdentifier:@"com.apple.springboard"];
    auto button = springboardApplication.buttons[@"Open"];
    if ([button waitForExistenceWithTimeout:
                    base::test::ios::kWaitForUIElementTimeout.InSecondsF()]) {
      [button tap];
    }
    [ChromeEarlGrey waitForMainTabCount:2];
  }
  [ChromeEarlGrey verifyActivitySheetNotVisible];
}

// Verifies that Tools Menu > Share Chrome brings up the "share sheet".
- (void)testShowShareSheetForChromeApp {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuShareChromeId)];
  [ChromeEarlGrey verifyActivitySheetVisible];
}

@end
