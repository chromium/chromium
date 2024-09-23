// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/strings/stringprintf.h"
#import "base/test/ios/wait_util.h"
#import "base/threading/platform_thread.h"
#import "base/time/time.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "url/gurl.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using chrome_test_util::ButtonWithAccessibilityLabelId;

namespace {

// Text appearing on the navigation test page.
const char kPageText[] = "Navigation testing page";

// Response provider that serves the page which never finishes loading.
// TODO(crbug.com/41311220): Convert this to Embedded Test Server.
class InfinitePendingResponseProvider : public HtmlResponseProvider {
 public:
  explicit InfinitePendingResponseProvider(const GURL& url) : url_(url) {}
  ~InfinitePendingResponseProvider() override {}

  // HtmlResponseProvider overrides:
  bool CanHandleRequest(const Request& request) override {
    return request.url == url_ ||
           request.url == GetInfinitePendingResponseUrl();
  }
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override {
    if (request.url == url_) {
      *headers = GetDefaultResponseHeaders();
      *response_body =
          base::StringPrintf("<p>%s</p><img src='%s'/>", kPageText,
                             GetInfinitePendingResponseUrl().spec().c_str());
    } else if (request.url == GetInfinitePendingResponseUrl()) {
      base::PlatformThread::Sleep(base::Days(1));
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

 private:
  // Returns a url for which this response provider will never reply.
  GURL GetInfinitePendingResponseUrl() const {
    GURL::Replacements replacements;
    replacements.SetPathStr("resource");
    return url_.DeprecatedGetOriginAsURL().ReplaceComponents(replacements);
  }

  // Main page URL that never finish loading.
  GURL url_;
};

// Waits for EG matcher element to be sufficiently visible. Useful when EG UI
// sync is disabled.
void WaitForMatcherVisible(id<GREYMatcher> matcher,
                           NSString* matcher_description) {
  ConditionBlock wait_for_matcher = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(
      WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, wait_for_matcher),
      @"Failed to wait %@ to be visible.", matcher_description);
}

}  // namespace

// Test case for Stop Loading button.
@interface StopLoadingTestCase : WebHttpServerChromeTestCase
@end

@implementation StopLoadingTestCase

// Tests that tapping "Stop" button stops the loading.
- (void)testStopLoading {
  // Load a page which never finishes loading.
  GURL infinitePendingURL = web::test::HttpServer::MakeUrl("http://infinite");
  web::test::SetUpHttpServer(
      std::make_unique<InfinitePendingResponseProvider>(infinitePendingURL));

  // EG synchronizes with WKWebView. Disable synchronization for EG interation
  // during when page is loading.
  ScopedSynchronizationDisabler disabler;

  [ChromeEarlGrey loadURL:infinitePendingURL waitForCompletion:NO];
  // Wait until the page is half loaded.
  [ChromeEarlGrey waitForWebStateContainingText:kPageText];
  if (![ChromeEarlGrey isIPadIdiom]) {
    // On iPhone Stop/Reload button is a part of tools menu, so open it.
    [ChromeEarlGreyUI openToolsMenu];
  }
  // Sleep for UI change because synchronization is disabled.
  base::PlatformThread::Sleep(base::Seconds(1));

  // Wait and verify that stop button is visible and reload button is hidden.
  WaitForMatcherVisible(chrome_test_util::StopButton(), @"stop button");
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ReloadButton()]
      assertWithMatcher:grey_notVisible()];

  // Stop the page loading.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::StopButton()]
      performAction:grey_tap()];
  // Sleep for UI change because synchronization is disabled.
  base::PlatformThread::Sleep(base::Seconds(1));
  if (![ChromeEarlGrey isIPadIdiom]) {
    // On iPhone Stop/Reload button is a part of tools menu, so open it.
    [ChromeEarlGreyUI openToolsMenu];
  }

  // Wait and verify that reload button is visible and stop button is hidden.
  WaitForMatcherVisible(chrome_test_util::ReloadButton(), @"reload button");
  [[EarlGrey selectElementWithMatcher:chrome_test_util::StopButton()]
      assertWithMatcher:grey_notVisible()];
}

@end
