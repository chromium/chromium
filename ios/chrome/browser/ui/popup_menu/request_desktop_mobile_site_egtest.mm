// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/features.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/web/common/features.h"
#include "ios/web/common/user_agent.h"
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
const char kDesktopPlatformLabel[] = "MacIntel";

// URL to be used when the page needs to be reloaded on back/forward
// navigations.
const char kPurgeURL[] = "url-purge.com";
// JavaScript used to reload the page on back/forward navigations.
const char kJavaScriptReload[] =
    "<script>window.onpageshow = function(event) {"
    "    if (event.persisted) {"
    "       window.location.href = window.location.href + \"?reloaded\""
    "    }"
    "};</script>";

// Custom timeout used when waiting for a web state after requesting desktop
// or mobile mode.
const NSTimeInterval kWaitForUserAgentChangeTimeout = 15.0;

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

    std::string purge_additions = "";
    if (request.url.path().find(kPurgeURL) != std::string::npos) {
      purge_additions = kJavaScriptReload;
    }

    *headers = web::ResponseProvider::GetDefaultResponseHeaders();
    std::string userAgent;
    std::string desktop_product =
        "CriOS/" + version_info::GetMajorVersionNumber();
    std::string desktop_user_agent =
        web::BuildDesktopUserAgent(desktop_product);
    if (request.headers.GetHeader("User-Agent", &userAgent) &&
        userAgent == desktop_user_agent) {
      response_body->assign(std::string(kDesktopSiteLabel) + "\n" +
                            purge_additions);
    } else {
      response_body->assign(std::string(kMobileSiteLabel) + "\n" +
                            purge_additions);
    }
  }
};
}  // namespace

// Tests for the tools popup menu.
@interface RequestDesktopMobileSiteTestCase : WebHttpServerChromeTestCase
@end

@implementation RequestDesktopMobileSiteTestCase

#pragma mark - Helpers

- (GREYElementInteraction*)defaultRequestButton {
  if ([ChromeEarlGrey isMobileModeByDefault])
    return RequestDesktopButton();
  return RequestMobileButton();
}

- (GREYElementInteraction*)nonDefaultRequestButton {
  if ([ChromeEarlGrey isMobileModeByDefault])
    return RequestMobileButton();
  return RequestDesktopButton();
}

- (std::string)defaultLabel {
  if ([ChromeEarlGrey isMobileModeByDefault])
    return kMobileSiteLabel;
  return kDesktopSiteLabel;
}

- (std::string)nonDefaultLabel {
  if ([ChromeEarlGrey isMobileModeByDefault])
    return kDesktopSiteLabel;
  return kMobileSiteLabel;
}

#pragma mark - Tests

// Tests that requesting desktop site of a page works and the user agent
// propagates to the next navigations in the same tab.
- (void)testRequestDesktopSitePropagatesToNextNavigations {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:[self defaultLabel]];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [[self defaultRequestButton] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:[self nonDefaultLabel]
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Verify that desktop user agent propagates.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://2.com")];
  [ChromeEarlGrey waitForWebStateContainingText:[self nonDefaultLabel]];
}

// Tests that requesting desktop site of a page works and the requested user
// agent is kept when restoring the session.
- (void)testRequestDesktopSiteKeptSessionRestoration {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:[self defaultLabel]];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [[self defaultRequestButton] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:[self nonDefaultLabel]
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Close all tabs and undo, trigerring a restoration.
  [ChromeEarlGrey triggerRestoreViaTabGridRemoveAllUndo];

  // Verify that desktop user agent propagates.
  [ChromeEarlGreyUI openToolsMenu];
  [[self nonDefaultRequestButton] assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForWebStateContainingText:[self nonDefaultLabel]];
}

// Tests that requesting desktop site of a page works and desktop user agent
// does not propagate to next the new tab.
- (void)testRequestDesktopSiteDoesNotPropagateToNewTab {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:[self defaultLabel]];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [[self defaultRequestButton] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:[self nonDefaultLabel]
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Verify that desktop user agent does not propagate to new tab.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://2.com")];
  [ChromeEarlGrey waitForWebStateContainingText:[self defaultLabel]];
}

// Tests that when requesting desktop on another page and coming back to a page
// that has been purged from memory, we still display the mobile page.
- (void)testRequestDesktopSiteGoBackToMobilePurged {
  if (@available(iOS 13, *)) {
  } else {
    EARL_GREY_TEST_DISABLED(@"On iOS 12, the User Agent can be wrong when "
                            @"doing back/forward navigations");
  }

  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(
                              "http://" + std::string(kPurgeURL))];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:[self defaultLabel]];

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://2.com")];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [[self defaultRequestButton] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:[self nonDefaultLabel]
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Verify that going back returns to the mobile site.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForPageLoadTimeout,
                 ^bool {
                   return [ChromeEarlGrey webStateVisibleURL].query() ==
                          "reloaded";
                 }),
             @"Page did not reload");
  [ChromeEarlGrey waitForWebStateContainingText:[self defaultLabel]];
}

// Tests that navigating forward to a page not using the default mode from a
// restored session is using the mode used in the past session.
- (void)testNavigateForwardToDesktopMode {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  // Load the page in the non-default mode.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  [ChromeEarlGrey waitForWebStateContainingText:[self defaultLabel]];

  [ChromeEarlGreyUI openToolsMenu];
  [[self defaultRequestButton] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:[self nonDefaultLabel]];

  [ChromeEarlGrey goBack];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Go back to NTP to restore the session from there.
  [ChromeEarlGrey triggerRestoreViaTabGridRemoveAllUndo];

  // Make sure that the NTP is displayed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // The session is restored, navigate forward and check the mode.
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:[self nonDefaultLabel]];
  [ChromeEarlGreyUI openToolsMenu];
  [[self nonDefaultRequestButton] assertWithMatcher:grey_notNil()];
}

// Tests that requesting mobile site of a page works and the user agent
// propagates to the next navigations in the same tab.
- (void)testRequestMobileSitePropagatesToNextNavigations {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://1.com")];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:[self defaultLabel]];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [[self defaultRequestButton] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:[self nonDefaultLabel]
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Request and verify reception of the mobile site.
  [ChromeEarlGreyUI openToolsMenu];
  [[self nonDefaultRequestButton] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:[self defaultLabel]
                                        timeout:kWaitForUserAgentChangeTimeout];

  // Verify that mobile user agent propagates.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl("http://2.com")];
  [ChromeEarlGrey waitForWebStateContainingText:[self defaultLabel]];
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
// mobile User Agent and the platform.
- (void)testAppVersionJSAPIWithMobileUserAgent {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kUserAgentTestURL)];
  // Verify initial reception of the mobile site.
  [ChromeEarlGrey waitForWebStateContainingText:[self defaultLabel]];

  std::string defaultPlatform;
  std::string nonDefaultPlatform;
  if ([ChromeEarlGrey isMobileModeByDefault]) {
    defaultPlatform = base::SysNSStringToUTF8([[UIDevice currentDevice] model]);
    if (@available(iOS 13, *)) {
      nonDefaultPlatform = kDesktopPlatformLabel;
    } else {
      nonDefaultPlatform = defaultPlatform;
    }
  } else {
    defaultPlatform = kDesktopPlatformLabel;
    nonDefaultPlatform =
        base::SysNSStringToUTF8([[UIDevice currentDevice] model]);
  }
  [ChromeEarlGrey waitForWebStateContainingText:defaultPlatform];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [[self defaultRequestButton] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:[self nonDefaultLabel]
                                        timeout:kWaitForUserAgentChangeTimeout];
  [ChromeEarlGrey waitForWebStateContainingText:nonDefaultPlatform];

  // Request and verify reception of the mobile site.
  [ChromeEarlGreyUI openToolsMenu];
  [[self nonDefaultRequestButton] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:[self defaultLabel]
                                        timeout:kWaitForUserAgentChangeTimeout];
  [ChromeEarlGrey waitForWebStateContainingText:defaultPlatform];
}

@end
