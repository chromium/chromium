// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import <memory>

#import "base/ios/ios_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
id<GREYMatcher> CopyButton() {
  return grey_allOf(
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_descendant(
          chrome_test_util::StaticTextWithAccessibilityLabel(@"Copy")),
      nil);
}

// Assert the activity service is visible by checking the "copy" button.
void AssertActivityServiceVisible() {
  [[EarlGrey selectElementWithMatcher:CopyButton()]
      assertWithMatcher:grey_interactable()];
}

// Assert the activity service is not visible by checking the "copy" button.
void AssertActivityServiceNotVisible() {
  [[EarlGrey selectElementWithMatcher:grey_allOf(CopyButton(),
                                                 grey_interactable(), nil)]
      assertWithMatcher:grey_nil()];
}

}  // namespace

// Earl grey integration tests for Activity Service Controller.
@interface ActivityServiceControllerTestCase : WebHttpServerChromeTestCase
@end

@implementation ActivityServiceControllerTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if (@available(iOS 15.0, *)) {
    config.features_enabled.push_back(kNewOverflowMenuShareChromeAction);
  }
  return config;
}

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
  AssertActivityServiceVisible();
  [[EarlGrey selectElementWithMatcher:CopyButton()]
      assertWithMatcher:grey_interactable()];

  // Start the Copy action and verify that the share menu gets dismissed.
  [[EarlGrey selectElementWithMatcher:CopyButton()] performAction:grey_tap()];
  AssertActivityServiceNotVisible();
}

// Verifies that Tools Menu > Share Chrome brings up the "share sheet".
- (void)testShareChromeApp {
  if (@available(iOS 15.0, *)) {
    [ChromeEarlGreyUI openToolsMenu];
    [ChromeEarlGreyUI
        tapToolsMenuAction:grey_accessibilityID(kToolsMenuShareChromeId)];
    AssertActivityServiceVisible();
  }
}

@end
