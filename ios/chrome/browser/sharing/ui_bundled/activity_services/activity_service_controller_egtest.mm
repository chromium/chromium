// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import <memory>

#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/popup_menu_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/error_page_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "ios/web/public/test/http_server/response_provider.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// The text of the Share Sheet item for the Open Extension.
NSString* const kEGOpenExtension = @"EGOpenExtension";
}  // namespace

// Earl grey integration tests for Activity Service Controller.
@interface ActivityServiceControllerTestCase : WebHttpServerChromeTestCase
@end

@implementation ActivityServiceControllerTestCase

- (void)testOpenActivityServiceControllerAndCopy {
  // Set up mock http server.
  std::map<GURL, std::string> responses;
  GURL url = web::test::HttpServer::MakeUrl("http://potato");
  responses[url] = "tomato";
  web::test::SetUpSimpleHttpServer(responses);

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
- (void)testOpenActivityServiceControllerAndOpenExtension {
  // EG does not support tapping on action extension before iOS17.
  if (@available(iOS 17.0, *)) {
    // Set up mock http server.
    std::map<GURL, std::string> responses;
    GURL url = web::test::HttpServer::MakeUrl("http://potato");
    responses[url] = "tomato";
    web::test::SetUpSimpleHttpServer(responses);

    // Open page and open the share menu.
    [ChromeEarlGrey loadURL:url];
    [ChromeEarlGreyUI openShareMenu];

    [ChromeEarlGrey verifyActivitySheetVisible];

    if (@available(iOS 19.0, *)) {
      // On iOS26 an additional click on "More" button needs to be performed.
      // Clicking on Activity Sheet doesn't work with EG if there are multiple
      // buttons with the same identifier, use XCUI directly.
      XCUIApplication* app = [[XCUIApplication alloc] init];
      XCUIElementQuery* more_buttons =
          [[app staticTexts] matchingIdentifier:@"More"];
      if (more_buttons.count == 2) {
        // There are two "More" buttons, select the one at the bottom.
        XCUIElement* more_button_0 = [more_buttons elementBoundByIndex:0];
        XCUIElement* more_button_1 = [more_buttons elementBoundByIndex:1];
        XCUIElement* more_button =
            more_button_0.frame.origin.x > more_button_1.frame.origin.x
                ? more_button_0
                : more_button_1;
        [more_button tap];
      } else if (more_buttons.count == 1) {
         XCUIElement* more_button = [more_buttons elementBoundByIndex:0];
         [more_button tap];
      }

      // TODO(crbug.com/432223861): Revisit using
      // `tapButtonInActivitySheetWithID` if it can be fixed to work better on
      // iOS 26, or if Apple fixes a bug that caused the item to not be
      // hittable.
      [ChromeEarlGrey verifyTextVisibleInActivitySheetWithID:kEGOpenExtension];
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
    if (![tabCountCheck
            waitWithTimeout:base::test::ios::kWaitForUIElementTimeout
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
}

// Verifies that Tools Menu > Share Chrome brings up the "share sheet".
- (void)testShowShareSheetForChromeApp {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuShareChromeId)];
  [ChromeEarlGrey verifyActivitySheetVisible];
}

@end
