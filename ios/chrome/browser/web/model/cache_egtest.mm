// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/scoped_block_popups_pref.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "url/gurl.h"

using web::test::HttpServer;

namespace {

// First page for cache testing.
const char kCacheTestFirstPageURL[] = "http://cacheTestFirstPage";

// Second page for cache testing.
const char kCacheTestSecondPageURL[] = "http://cacheTestSecondPage";

// Third page for cache testing.
const char kCacheTestThirdPageURL[] = "http://cacheTestThirdPage";

// ID for HTML hyperlink element.
const char kCacheTestLinkID[] = "cache-test-link-id";

// Response provider for cache testing that provides server hit count and
// cache-control request header.
class CacheTestResponseProvider : public web::DataResponseProvider {
 public:
  CacheTestResponseProvider()
      : first_page_url_(HttpServer::MakeUrl(kCacheTestFirstPageURL)),
        second_page_url_(HttpServer::MakeUrl(kCacheTestSecondPageURL)),
        third_page_url_(HttpServer::MakeUrl(kCacheTestThirdPageURL)) {}
  ~CacheTestResponseProvider() override {}

  // HtmlResponseProvider overrides:
  bool CanHandleRequest(const Request& request) override {
    return request.url == first_page_url_ || request.url == second_page_url_ ||
           request.url == third_page_url_;
  }
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override {
    hit_counter_++;
    std::string cache_control_header;
    if (request.headers.HasHeader("Cache-Control")) {
      request.headers.GetHeader("Cache-Control", &cache_control_header);
    }
    *headers = web::ResponseProvider::GetDefaultResponseHeaders();

    if (request.url == first_page_url_) {
      *response_body = base::StringPrintf(
          "<p>First Page</p>"
          "<p>serverHitCounter: %d</p>"
          "<p>cacheControl: %s</p>"
          "<a href='%s' id='%s'>link to second page</a>",
          hit_counter_, cache_control_header.c_str(),
          second_page_url_.spec().c_str(), kCacheTestLinkID);

    } else if (request.url == second_page_url_) {
      *response_body = base::StringPrintf(
          "<p>Second Page</p>"
          "<p>serverHitCounter: %d</p>"
          "<p>cacheControl: %s</p>",
          hit_counter_, cache_control_header.c_str());
    } else if (request.url == third_page_url_) {
      *response_body = base::StringPrintf(
          "<p>Third Page</p>"
          "<p>serverHitCounter: %d</p>"
          "<p>cacheControl: %s</p>"
          "<a href='%s' id='%s' target='_blank'>"
          "link to first page in new tab</a>",
          hit_counter_, cache_control_header.c_str(),
          first_page_url_.spec().c_str(), kCacheTestLinkID);
    } else {
      NOTREACHED();
    }
  }

 private:
  // A number that counts requests that have reached the server.
  int hit_counter_ = 0;

  // URLs for three test pages.
  GURL first_page_url_;
  GURL second_page_url_;
  GURL third_page_url_;
};

}  // namespace

// Tests the browser cache behavior when navigating to cached pages.
@interface CacheTestCase : WebHttpServerChromeTestCase
@end

@implementation CacheTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  // Features are enabled or disabled based on the name of the test that is
  // running. This is done because it is inefficient to use
  // ensureAppLaunchedWithConfiguration for each test.
  if ([self isRunningTest:@selector
            (testCachingBehaviorOnSelectOmniboxSuggestion)]) {
    // Explicitly disable feature OnDeviceHeadProviderNonIncognito, whose delay
    // (i.e. http://shortn/_o7kPJvU8ac) will otherwise cause flakiness for this
    // test in build iphone-device (crbug.com/1153136).
    config.features_disabled.push_back(
        omnibox::kOnDeviceHeadProviderNonIncognito);
  }
  return config;
}

// Tests caching behavior when opening new tab. New tab should not use the
// cached page.
- (void)testCachingBehaviorOnOpenNewTab {
  web::test::SetUpHttpServer(std::make_unique<CacheTestResponseProvider>());

  const GURL cacheTestFirstPageURL =
      HttpServer::MakeUrl(kCacheTestFirstPageURL);
  const GURL cacheTestThirdPageURL =
      HttpServer::MakeUrl(kCacheTestThirdPageURL);

  // 1st hit to server. Verify title and hitCount.
  [ChromeEarlGrey loadURL:cacheTestFirstPageURL];
  [ChromeEarlGrey waitForWebStateContainingText:"First Page"];
  [ChromeEarlGrey waitForWebStateContainingText:"serverHitCounter: 1"];

  // 2nd hit to server. Verify hitCount.
  [ChromeEarlGrey loadURL:cacheTestThirdPageURL];
  [ChromeEarlGrey waitForWebStateContainingText:"serverHitCounter: 2"];

  // Open the first page in a new tab. Verify that cache was not used. Must
  // first allow popups.
  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_ALLOW);
  [ChromeEarlGrey
      tapWebStateElementWithID:[NSString
                                   stringWithUTF8String:kCacheTestLinkID]];
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey waitForPageToFinishLoading];
  [ChromeEarlGrey waitForWebStateContainingText:"First Page"];
  [ChromeEarlGrey waitForWebStateContainingText:"serverHitCounter: 3"];
}

// Tests that cache is not used when selecting omnibox suggested website, even
// though cache for that website exists.
- (void)testCachingBehaviorOnSelectOmniboxSuggestion {
  web::test::SetUpHttpServer(std::make_unique<CacheTestResponseProvider>());

  // Clear the history to ensure expected omnibox autocomplete results.
  [ChromeEarlGrey clearBrowsingHistory];

  const GURL cacheTestFirstPageURL =
      HttpServer::MakeUrl(kCacheTestFirstPageURL);

  // 1st hit to server. Verify title and hitCount.
  [ChromeEarlGrey loadURL:cacheTestFirstPageURL];
  [ChromeEarlGrey waitForWebStateContainingText:"First Page"];
  [ChromeEarlGrey waitForWebStateContainingText:"serverHitCounter: 1"];

  // Type a search into omnnibox and select the first suggestion (second row)
  [ChromeEarlGreyUI focusOmniboxAndType:@"cachetestfirstpage"];
  [[[[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::OmniboxPopupRow(),
                     grey_descendant(
                         chrome_test_util::StaticTextWithAccessibilityLabel(
                             base::SysUTF8ToNSString(
                                 cacheTestFirstPageURL.GetContent()))),
                     grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:chrome_test_util::OmniboxPopupList()]
      assertWithMatcher:grey_sufficientlyVisible()] performAction:grey_tap()];

  // Verify title and hitCount. Cache should not be used.
  [ChromeEarlGrey waitForWebStateContainingText:"First Page"];
  [ChromeEarlGrey waitForWebStateContainingText:"serverHitCounter: 2"];
}

@end
