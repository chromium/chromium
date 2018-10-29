// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/chrome_web_view_factory.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/web/public/test/http_server/data_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kUserAgentTestURL[] =
    "http://ios/testing/data/http_server_files/user_agent_test_page.html";

const char kMobileSiteLabel[] = "Mobile";

const char kDesktopSiteLabel[] = "Desktop";

// Select the button to request desktop site by scrolling the collection.
// 200 is a reasonable scroll displacement that works for all UI elements, while
// not being too slow.
GREYElementInteraction* RequestDesktopButton() {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kToolsMenuRequestDesktopId),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kPopupMenuToolsMenuTableViewId)];
}

// Select the button to request mobile site by scrolling the collection.
// 200 is a reasonable scroll displacement that works for all UI elements, while
// not being too slow.
GREYElementInteraction* RequestMobileButton() {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kToolsMenuRequestMobileId),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kPopupMenuToolsMenuTableViewId)];
}

// A ResponseProvider that provides user agent for httpServer request.
class UserAgentResponseProvider : public web::DataResponseProvider {
 public:
  bool CanHandleRequest(const Request& request) override { return true; }

  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override {
    // Do not return anything if static plist file has been requested,
    // as plain text is not a valid property list content.
    if ([[base::SysUTF8ToNSString(request.url.spec()) pathExtension]
            isEqualToString:@"plist"]) {
      *headers =
          web::ResponseProvider::GetResponseHeaders("", net::HTTP_NO_CONTENT);
      return;
    }

    *headers = web::ResponseProvider::GetDefaultResponseHeaders();
    std::string userAgent;
    const std::string kDesktopUserAgent =
        base::SysNSStringToUTF8(ChromeWebView::kDesktopUserAgent);
    if (request.headers.GetHeader("User-Agent", &userAgent) &&
        userAgent == kDesktopUserAgent) {
      response_body->assign("Desktop");
    } else {
      response_body->assign("Mobile");
    }
  }
};
}  // namespace

// Tests for the tools popup menu.
@interface RequestDesktopMobileSiteTestCase : ChromeTestCase
@end

@implementation RequestDesktopMobileSiteTestCase

// Tests that requesting desktop site of a page works and the user agent
// propagates to the next navigations in the same tab.
- (void)testRequestDesktopSitePropagatesToNextNavigations {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebViewContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebViewContainingText:kDesktopSiteLabel];

  // Verify that desktop user agent propagates.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://2.com")];
  [ChromeEarlGrey waitForWebViewContainingText:kDesktopSiteLabel];
}

// Tests that requesting desktop site of a page works and desktop user agent
// does not propagate to next the new tab.
- (void)testRequestDesktopSiteDoesNotPropagateToNewTab {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebViewContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebViewContainingText:kDesktopSiteLabel];

  // Verify that desktop user agent does not propagate to new tab.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://2.com")];
  [ChromeEarlGrey waitForWebViewContainingText:kMobileSiteLabel];
}

// Tests that requesting desktop site of a page works and going back re-opens
// mobile version of the page.
- (void)testRequestDesktopSiteGoBackToMobile {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebViewContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebViewContainingText:kDesktopSiteLabel];

  // Verify that going back returns to the mobile site.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebViewContainingText:kMobileSiteLabel];
}

// Tests that requesting mobile site of a page works and the user agent
// propagates to the next navigations in the same tab.
- (void)testRequestMobileSitePropagatesToNextNavigations {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebViewContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebViewContainingText:kDesktopSiteLabel];

  // Request and verify reception of the mobile site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestMobileButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebViewContainingText:kMobileSiteLabel];

  // Verify that mobile user agent propagates.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://2.com")];
  [ChromeEarlGrey waitForWebViewContainingText:kMobileSiteLabel];
}

// Tests that requesting mobile site of a page works and going back re-opens
// desktop version of the page.
- (void)testRequestMobileSiteGoBackToDesktop {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebViewContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebViewContainingText:kDesktopSiteLabel];

  // Request and verify reception of the mobile site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestMobileButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebViewContainingText:kMobileSiteLabel];

  // Verify that going back returns to the desktop site.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebViewContainingText:kDesktopSiteLabel];
}

// Tests that requesting desktop site button is not enabled on new tab pages.
- (void)testRequestDesktopSiteNotEnabledOnNewTabPage {
  // Verify tapping on request desktop button is no-op.
  [ChromeEarlGreyUI openToolsMenu];
  [[RequestDesktopButton() assertWithMatcher:grey_notNil()]
      performAction:grey_tap()];
  [RequestDesktopButton() assertWithMatcher:grey_notNil()];
}

// Tests that requesting desktop site button is not enabled on WebUI pages.
- (void)testRequestDesktopSiteNotEnabledOnWebUIPage {
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  // Verify tapping on request desktop button is no-op.
  [ChromeEarlGreyUI openToolsMenu];
  [[RequestDesktopButton() assertWithMatcher:grey_notNil()]
      performAction:grey_tap()];
  [RequestDesktopButton() assertWithMatcher:grey_notNil()];
}

// Tests that navigator.appVersion JavaScript API returns correct string for
// desktop User Agent.
- (void)testAppVersionJSAPIWithDesktopUserAgent {
  web::test::SetUpFileBasedHttpServer();
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kUserAgentTestURL)];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebViewContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebViewContainingText:kDesktopSiteLabel];
}

// Tests that navigator.appVersion JavaScript API returns correct string for
// mobile User Agent.
- (void)testAppVersionJSAPIWithMobileUserAgent {
  web::test::SetUpFileBasedHttpServer();
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kUserAgentTestURL)];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebViewContainingText:kMobileSiteLabel];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebViewContainingText:kDesktopSiteLabel];

  // Request and verify reception of the mobile site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestMobileButton() performAction:grey_tap()];
  [ChromeEarlGrey waitForWebViewContainingText:kMobileSiteLabel];
}

@end
