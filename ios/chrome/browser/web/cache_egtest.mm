// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/ios/ios_util.h"
#include "base/strings/stringprintf.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/chrome/test/earl_grey/scoped_block_popups_pref.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
@interface CacheTestCase : ChromeTestCase
@end

@implementation CacheTestCase

// Tests caching behavior on navigate back and page reload. Navigate back should
// use the cached page. Page reload should use cache-control in the request
// header and show updated page.
- (void)testCachingBehaviorOnNavigateBackAndPageReload {
  // TODO(crbug.com/747436): re-enable this test on iOS 10.3.1 and afterwards
  // once the bug is fixed.
  if (base::ios::IsRunningOnOrLater(10, 3, 1)) {
    EARL_GREY_TEST_DISABLED(@"Disabled on iOS 10.3.1 and afterwards.");
  }

  web::test::SetUpHttpServer(std::make_unique<CacheTestResponseProvider>());

  const GURL cacheTestFirstPageURL =
      HttpServer::MakeUrl(kCacheTestFirstPageURL);

  // 1st hit to server. Verify that the server has the correct hit count.
  [ChromeEarlGrey loadURL:cacheTestFirstPageURL];
  [ChromeEarlGrey waitForWebStateContainingText:"serverHitCounter: 1"];

  // Navigate to another page. 2nd hit to server.
  [ChromeEarlGrey
      tapWebStateElementWithID:[NSString
                                   stringWithUTF8String:kCacheTestLinkID]];
  [ChromeEarlGrey waitForWebStateContainingText:"serverHitCounter: 2"];

  // Navigate back. This should not hit the server. Verify the page has been
  // loaded from cache. The serverHitCounter will remain the same.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:"serverHitCounter: 1"];

  // Reload page. 3rd hit to server. Verify that page reload causes the
  // hitCounter to show updated value.
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:"serverHitCounter: 3"];

  // Verify that page reload causes Cache-Control value to be sent with request.
  [ChromeEarlGrey waitForWebStateContainingText:"cacheControl: max-age=0"];
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
  // TODO(crbug.com/753098): Re-enable this test on iPad once grey_typeText
  // works.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }

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
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"omnibox suggestion 1")]
      performAction:grey_tap()];

  // Verify title and hitCount. Cache should not be used.
  [ChromeEarlGrey waitForWebStateContainingText:"First Page"];
  [ChromeEarlGrey waitForWebStateContainingText:"serverHitCounter: 2"];
}

@end
