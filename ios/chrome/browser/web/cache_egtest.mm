// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include <memory>

#include "base/ios/ios_util.h"
#include "base/strings/stringprintf.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/app/history_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#import "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/chrome/test/scoped_block_popups_pref.h"
#include "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::GetOriginalBrowserState;
using chrome_test_util::TapWebViewElementWithId;
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
  [ChromeEarlGrey waitForWebViewContainingText:"serverHitCounter: 1"];

  // Navigate to another page. 2nd hit to server.
  GREYAssert(chrome_test_util::TapWebViewElementWithId(kCacheTestLinkID),
             @"Failed to tap %s", kCacheTestLinkID);
  [ChromeEarlGrey waitForWebViewContainingText:"serverHitCounter: 2"];

  // Navigate back. This should not hit the server. Verify the page has been
  // loaded from cache. The serverHitCounter will remain the same.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebViewContainingText:"serverHitCounter: 1"];

  // Reload page. 3rd hit to server. Verify that page reload causes the
  // hitCounter to show updated value.
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebViewContainingText:"serverHitCounter: 3"];

  // Verify that page reload causes Cache-Control value to be sent with request.
  [ChromeEarlGrey waitForWebViewContainingText:"cacheControl: max-age=0"];
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
  [ChromeEarlGrey waitForWebViewContainingText:"First Page"];
  [ChromeEarlGrey waitForWebViewContainingText:"serverHitCounter: 1"];

  // 2nd hit to server. Verify hitCount.
  [ChromeEarlGrey loadURL:cacheTestThirdPageURL];
  [ChromeEarlGrey waitForWebViewContainingText:"serverHitCounter: 2"];

  // Open the first page in a new tab. Verify that cache was not used. Must
  // first allow popups.
  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_ALLOW,
                                   GetOriginalBrowserState());
  GREYAssert(chrome_test_util::TapWebViewElementWithId(kCacheTestLinkID),
             @"Failed to tap %s", kCacheTestLinkID);
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey waitForPageToFinishLoading];
  [ChromeEarlGrey waitForWebViewContainingText:"First Page"];
  [ChromeEarlGrey waitForWebViewContainingText:"serverHitCounter: 3"];
}

// Tests that cache is not used when selecting omnibox suggested website, even
// though cache for that website exists.
- (void)testCachingBehaviorOnSelectOmniboxSuggestion {
  // TODO(crbug.com/753098): Re-enable this test on iOS 11 iPad once
  // grey_typeText works on iOS 11.
  if (base::ios::IsRunningOnIOS11OrLater() && IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 11.");
  }

  web::test::SetUpHttpServer(std::make_unique<CacheTestResponseProvider>());

  // Clear the history to ensure expected omnibox autocomplete results.
  GREYAssertTrue(chrome_test_util::ClearBrowsingHistory(),
                 @"Clearing Browsing History timed out");

  const GURL cacheTestFirstPageURL =
      HttpServer::MakeUrl(kCacheTestFirstPageURL);

  // 1st hit to server. Verify title and hitCount.
  [ChromeEarlGrey loadURL:cacheTestFirstPageURL];
  [ChromeEarlGrey waitForWebViewContainingText:"First Page"];
  [ChromeEarlGrey waitForWebViewContainingText:"serverHitCounter: 1"];

  // Type a search into omnnibox and select the first suggestion (second row)
  [ChromeEarlGreyUI focusOmniboxAndType:@"cachetestfirstpage"];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"omnibox suggestion 1")]
      performAction:grey_tap()];

  // Verify title and hitCount. Cache should not be used.
  [ChromeEarlGrey waitForWebViewContainingText:"First Page"];
  [ChromeEarlGrey waitForWebViewContainingText:"serverHitCounter: 2"];
}

@end
