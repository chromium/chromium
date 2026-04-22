// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

using chrome_test_util::ClearBrowsingDataButton;
using chrome_test_util::ClearBrowsingDataCell;
using chrome_test_util::ClearBrowsingDataView;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuPrivacyButton;

namespace {

// Matcher for a tile containing `text`.
id<GREYMatcher> TileWithText(NSString* text) {
  return grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(text),
                    grey_ancestor(grey_kindOfClassName(@"MostVisitedTileView")),
                    nil);
}

std::unique_ptr<net::test_server::HttpResponse> CreateHttpResponse(
    const std::string& content) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(content);
  return response;
}

std::unique_ptr<net::test_server::HttpResponse> NotFoundResponse() {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_NOT_FOUND);
  return response;
}

// Handles requests for the NTP tiles test.
std::unique_ptr<net::test_server::HttpResponse> HandleNTPTilesRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == "/simple_tile.html") {
    return CreateHttpResponse("<head><title>title1</title></head>"
                              "<body>You are here.</body>");
  }
  if (request.relative_url == "/firstRedirect/") {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_MOVED_PERMANENTLY);
    response->AddCustomHeader("Location", "/destination/");
    response->set_content("<head><title>title1</title></head>"
                          "<body>Should redirect away.</body>");
    response->set_content_type("text/html");
    return response;
  }
  if (request.relative_url == "/destination/") {
    return CreateHttpResponse("<head><title>title2</title></head>"
                              "<body>redirect complete</body>");
  }
  return NotFoundResponse();
}

}  // namespace

// Test case for NTP tiles.
@interface NTPTilesTest : ChromeTestCase
@end

@implementation NTPTilesTest

- (void)setUp {
  [super setUp];

  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&HandleNTPTilesRequest));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (void)tearDownHelper {
  [ChromeEarlGrey clearBrowsingHistory];
  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

// Tests that loading a URL ends up creating an NTP tile and shows it on cold
// start.
- (void)testTopSitesTileAfterLoadURLAndColdStart {
  const GURL URL = self.testServer->GetURL("/simple_tile.html");

  // Clear history and verify that the tile does not exist.
  if (![ChromeTestCase forceRestartAndWipe]) {
    [ChromeEarlGrey clearBrowsingHistory];
  }

  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];

  [[EarlGrey selectElementWithMatcher:TileWithText(@"title1")]
      assertWithMatcher:grey_nil()];

  [ChromeEarlGrey loadURL:URL];

  [ChromeEarlGrey goBack];
  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey selectElementWithMatcher:TileWithText(@"title1")]
      assertWithMatcher:grey_notNil()];

  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:self.appConfigurationForTestCase];
  [[EarlGrey selectElementWithMatcher:TileWithText(@"title1")]
      assertWithMatcher:grey_notNil()];
}

// Tests that only one NTP tile is displayed for a TopSite that involves a
// redirect.
- (void)testTopSitesTileAfterRedirect {
  const GURL firstRedirectURL = self.testServer->GetURL("/firstRedirect/");
  const GURL destinationURL = self.testServer->GetURL("/destination/");

  // Clear history and verify that the tile does not exist.
  [ChromeEarlGrey clearBrowsingHistory];
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];
  [[EarlGrey selectElementWithMatcher:TileWithText(@"title2")]
      assertWithMatcher:grey_nil()];

  // Load first URL and expect redirect to destination URL.
  [ChromeEarlGrey loadURL:firstRedirectURL];
  [ChromeEarlGrey waitForWebStateContainingText:"redirect complete"];

  [ChromeEarlGrey goBack];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Which of the two tiles that is displayed is an implementation detail, and
  // this test helps document it. The purpose of the test is to verify that only
  // one tile is displayed.
  [[EarlGrey selectElementWithMatcher:TileWithText(@"title2")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TileWithText(@"title1")]
      assertWithMatcher:grey_nil()];

  // Clear history and verify that the tile does not exist.
  [ChromeEarlGrey clearBrowsingHistory];

  // Wait for clear browsing data to completed before checking for title2 to
  // disappear.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:TileWithText(@"title2")
                                     timeout:
                                         base::test::ios::
                                             kWaitForClearBrowsingDataTimeout];
}

@end
