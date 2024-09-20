// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/html_response_provider_impl.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"

using chrome_test_util::ClearBrowsingDataCell;
using chrome_test_util::ClearBrowsingDataView;
using chrome_test_util::ClearBrowsingDataButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuPrivacyButton;
using web::test::HttpServer;

// Test case for NTP tiles.
@interface NTPTilesTest : WebHttpServerChromeTestCase
@end

@implementation NTPTilesTest

- (void)tearDown {
  [ChromeEarlGrey clearBrowsingHistory];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

// Tests that loading a URL ends up creating an NTP tile and shows it on cold
// start.
- (void)testTopSitesTileAfterLoadURLAndColdStart {
  std::map<GURL, std::string> responses;
  GURL URL = web::test::HttpServer::MakeUrl("http://simple_tile.html");
  responses[URL] =
      "<head><title>title1</title></head>"
      "<body>You are here.</body>";
  web::test::SetUpSimpleHttpServer(responses);

  // Clear history and verify that the tile does not exist.
  [ChromeEarlGrey clearBrowsingHistory];
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(@"title1")]
      assertWithMatcher:grey_nil()];

  [ChromeEarlGrey loadURL:URL];

  [ChromeEarlGrey goBack];
  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(@"title1")]
      assertWithMatcher:grey_notNil()];

  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:self.appConfigurationForTestCase];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(@"title1")]
      assertWithMatcher:grey_notNil()];
}

// Tests that only one NTP tile is displayed for a TopSite that involves a
// redirect.
- (void)testTopSitesTileAfterRedirect {
  std::map<GURL, HtmlResponseProviderImpl::Response> responses;
  const GURL firstRedirectURL = HttpServer::MakeUrl("http://firstRedirect/");
  const GURL destinationURL = HttpServer::MakeUrl("http://destination/");
  responses[firstRedirectURL] = HtmlResponseProviderImpl::GetRedirectResponse(
      destinationURL, net::HTTP_MOVED_PERMANENTLY);

  // Add titles to both responses, which is what will show up on the NTP.
  responses[firstRedirectURL].body =
      "<head><title>title1</title></head>"
      "<body>Should redirect away.</body>";

  const char kFinalPageContent[] =
      "<head><title>title2</title></head>"
      "<body>redirect complete</body>";
  responses[destinationURL] =
      HtmlResponseProviderImpl::GetSimpleResponse(kFinalPageContent);
  std::unique_ptr<web::DataResponseProvider> provider(
      new HtmlResponseProvider(responses));
  web::test::SetUpHttpServer(std::move(provider));

  // Clear history and verify that the tile does not exist.
  [ChromeEarlGrey clearBrowsingHistory];
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(@"title2")]
      assertWithMatcher:grey_nil()];

  // Load first URL and expect redirect to destination URL.
  [ChromeEarlGrey loadURL:firstRedirectURL];
  [ChromeEarlGrey waitForWebStateContainingText:"redirect complete"];

  [ChromeEarlGrey goBack];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Which of the two tiles that is displayed is an implementation detail, and
  // this test helps document it. The purpose of the test is to verify that only
  // one tile is displayed.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(@"title2")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(@"title1")]
      assertWithMatcher:grey_nil()];

  // Clear history and verify that the tile does not exist.
  [ChromeEarlGrey clearBrowsingHistory];

  // Wait for clear browsing data to completed before checking for title2 to
  // disappear.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          chrome_test_util::StaticTextWithAccessibilityLabel(@"title2")
                                     timeout:
                                         base::test::ios::
                                             kWaitForClearBrowsingDataTimeout];
}

@end
