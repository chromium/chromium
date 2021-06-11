// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <WebKit/WebKit.h>

#include "base/strings/utf_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#include "ios/testing/embedded_test_server_handlers.h"
#include "ios/web/public/browsing_data/cookie_blocking_mode.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/js_test_storage_util.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "ios/web/public/web_state.h"
#include "net/base/escape.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace {
// Page with text "Main frame body" and iframe with src URL equal to the URL
// query string.
const char kPageUrl[] = "/iframe?";
// URL of iframe.
const char kIFrameUrl[] = "/foo";

NSString* const kLocalStorageErrorMessage =
    @"Failed to read the 'localStorage' property from 'window': Access is "
    @"denied for this document";
NSString* const kSessionStorageErrorMessage =
    @"Failed to read the 'sessionStorage' property from 'window': Access is "
    @"denied for this document";
NSString* const kCacheNotAvailableErrorMessage = @"Can't find variable: caches";
NSString* const kCacheErrorMessage = @"An attempt was made to break through "
                                     @"the security policy of the user agent.";
NSString* const kIndexedDBErrorMessage = @"Can't find variable: indexedDB";
}

namespace web {

// A test fixture for testing that the cookie blocking feature works correctly.
class CookieBlockingTest : public WebTestWithWebState {
 protected:
  CookieBlockingTest() : WebTestWithWebState() {}
  CookieBlockingTest(const CookieBlockingTest&) = delete;
  CookieBlockingTest& operator=(const CookieBlockingTest&) = delete;


  void SetUp() override {
    WebTestWithWebState::SetUp();
    server_.RegisterRequestHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/iframe",
                            base::BindRepeating(&testing::HandleIFrame)));
    server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/set-cookies",
        base::BindRepeating(&CookieBlockingTest::HandleSetCookiesRequest,
                            base::Unretained(this))));
    server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/get-cookies",
        base::BindRepeating(&CookieBlockingTest::HandleGetCookiesRequest,
                            base::Unretained(this))));

    third_party_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/set-cookies",
        base::BindRepeating(&CookieBlockingTest::HandleSetCookiesRequest,
                            base::Unretained(this))));
    third_party_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/get-cookies",
        base::BindRepeating(&CookieBlockingTest::HandleGetCookiesRequest,
                            base::Unretained(this))));
    ASSERT_TRUE(server_.Start());
    ASSERT_TRUE(third_party_server_.Start());
  }

  std::unique_ptr<HttpResponse> HandleSetCookiesRequest(
      const HttpRequest& request) {
    auto http_response = std::make_unique<BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("set-cookies");
    http_response->set_content_type("text/plain");
    http_response->AddCustomHeader("Set-Cookie", "a=b");
    return http_response;
  }

  std::unique_ptr<HttpResponse> HandleGetCookiesRequest(
      const HttpRequest& request) {
    auto http_response = std::make_unique<BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("get-cookies");
    http_response->set_content_type("text/plain");
    auto pos = request.headers.find("Cookie");
    if (pos == request.headers.end()) {
      latest_get_cookies_ = "";
    } else {
      latest_get_cookies_ = pos->second;
    }
    return http_response;
  }

  std::string FailureMessage(WebFrame* frame) {
    std::string message = "Failure in ";
    message += (frame->IsMainFrame() ? "main frame." : "child frame.");
    return message;
  }

  net::EmbeddedTestServer server_;
  net::EmbeddedTestServer third_party_server_;

  // Holds the cookies provided in the latest request to /get-cookies.
  // The requests are asynchronous, so the cookies in the request need to be
  // saved here so they can be checked in the test.
  std::string latest_get_cookies_;
};

// Tests that cookies are accessible from JavaScript in all frames
// when the blocking mode is set to allow.
TEST_F(CookieBlockingTest, CookiesAllowed) {
  bool success = false;
  GetBrowserState()->SetCookieBlockingMode(
      CookieBlockingMode::kAllow,
      base::BindLambdaForTesting([&] { success = true; }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL(kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  for (WebFrame* frame :
       web_state()->GetWebFramesManager()->GetAllWebFrames()) {
    EXPECT_TRUE(
        web::test::SetCookie(frame, @"someCookieName", @"someCookieValue"))
        << FailureMessage(frame);

    NSString* result;
    EXPECT_TRUE(web::test::GetCookies(frame, &result)) << FailureMessage(frame);
    EXPECT_NSEQ(result, @"someCookieName=someCookieValue")
        << FailureMessage(frame);
  }
}

// Tests that cookies are inaccessable from JavaScript in all frames
// when the blocking mode is set to block.
TEST_F(CookieBlockingTest, CookiesBlocked) {
  __block bool success = false;
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlock,
                                           base::BindOnce(^{
                                             success = true;
                                           }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL("localhost", kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  for (WebFrame* frame :
       web_state()->GetWebFramesManager()->GetAllWebFrames()) {
    EXPECT_TRUE(
        web::test::SetCookie(frame, @"someCookieName", @"someCookieValue"))
        << FailureMessage(frame);

    NSString* result;
    EXPECT_TRUE(web::test::GetCookies(frame, &result)) << FailureMessage(frame);
    EXPECT_NSEQ(result, @"") << FailureMessage(frame);
  }
}

// Tests that cookies are accessible from JavaScript on the main page, but
// inaccessible from a third-party iframe when third party cookies are blocked.
TEST_F(CookieBlockingTest, ThirdPartyCookiesBlocked) {
  __block bool success = false;
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlockThirdParty,
                                           base::BindOnce(^{
                                             success = true;
                                           }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL("localhost", kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  for (WebFrame* frame :
       web_state()->GetWebFramesManager()->GetAllWebFrames()) {
    EXPECT_TRUE(
        web::test::SetCookie(frame, @"someCookieName", @"someCookieValue"))
        << FailureMessage(frame);

    NSString* result;
    EXPECT_TRUE(web::test::GetCookies(frame, &result)) << FailureMessage(frame);
    if (frame->IsMainFrame()) {
      EXPECT_NSEQ(result, @"someCookieName=someCookieValue")
          << FailureMessage(frame);
    } else {
      EXPECT_NSEQ(result, @"") << FailureMessage(frame);
    }
  }
}

// Tests that a first-party iframe can still access cookies when third party
// cookies are blocked.
TEST_F(CookieBlockingTest, FirstPartyCookiesNotBlockedWhenThirdPartyBlocked) {
  __block bool success = false;
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlockThirdParty,
                                           base::BindOnce(^{
                                             success = true;
                                           }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  GURL iframe_url = server_.GetURL(kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  for (WebFrame* frame :
       web_state()->GetWebFramesManager()->GetAllWebFrames()) {
    EXPECT_TRUE(
        web::test::SetCookie(frame, @"someCookieName", @"someCookieValue"))
        << FailureMessage(frame);

    NSString* result;
    EXPECT_TRUE(web::test::GetCookies(frame, &result)) << FailureMessage(frame);
    EXPECT_NSEQ(result, @"someCookieName=someCookieValue")
        << FailureMessage(frame);
  }
}

// Tests that the document.cookie override cannot be deleted by external
// JavaScript.
TEST_F(CookieBlockingTest, CookiesBlockedUndeletable) {
  __block bool success = false;
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlock,
                                           base::BindOnce(^{
                                             success = true;
                                           }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL("localhost", kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  web_state()->ExecuteJavaScript(u"delete document.cookie");

  WebFrame* main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
  EXPECT_TRUE(web::test::SetCookie(main_frame, @"x", @"value"));

  NSString* result;
  EXPECT_TRUE(web::test::GetCookies(main_frame, &result));
  EXPECT_NSEQ(result, @"");
}

// Tests that localStorage is accessible from JavaScript in all frames
// when the blocking mode is set to allow.
TEST_F(CookieBlockingTest, LocalStorageAllowed) {
  __block bool success = false;
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kAllow,
                                           base::BindOnce(^{
                                             success = true;
                                           }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL("localhost", kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  for (WebFrame* frame :
       web_state()->GetWebFramesManager()->GetAllWebFrames()) {
    NSString* error_message;
    EXPECT_TRUE(
        web::test::SetLocalStorage(frame, @"x", @"value", &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(nil, error_message) << FailureMessage(frame);

    error_message = nil;
    NSString* result;
    EXPECT_TRUE(
        web::test::GetLocalStorage(frame, @"x", &result, &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(nil, error_message) << FailureMessage(frame);
    EXPECT_NSEQ(@"value", result) << FailureMessage(frame);
  }
}

// Tests that localStorage is inaccessable from JavaScript in all frames
// when the blocking mode is set to block.
TEST_F(CookieBlockingTest, LocalStorageBlocked) {
  __block bool success = false;
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlock,
                                           base::BindOnce(^{
                                             success = true;
                                           }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL("localhost", kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  for (WebFrame* frame :
       web_state()->GetWebFramesManager()->GetAllWebFrames()) {
    NSString* error_message;
    EXPECT_TRUE(
        web::test::SetLocalStorage(frame, @"x", @"value", &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(error_message, kLocalStorageErrorMessage)
        << FailureMessage(frame);

    error_message = nil;
    NSString* result;
    EXPECT_TRUE(
        web::test::GetLocalStorage(frame, @"x", &result, &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(error_message, kLocalStorageErrorMessage)
        << FailureMessage(frame);
    EXPECT_NSEQ(nil, result) << FailureMessage(frame);
  }
}

// Tests that the localStorage override is undeletable via extra JavaScript.
TEST_F(CookieBlockingTest, LocalStorageBlockedUndeletable) {
  __block bool success = false;
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlock,
                                           base::BindOnce(^{
                                             success = true;
                                           }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL("localhost", kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  web_state()->ExecuteJavaScript(u"delete localStorage");

  WebFrame* main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();

  NSString* error_message;
  EXPECT_TRUE(
      web::test::SetLocalStorage(main_frame, @"x", @"value", &error_message));
  EXPECT_NSEQ(error_message, kLocalStorageErrorMessage);

  error_message = nil;
  NSString* result;
  EXPECT_TRUE(
      web::test::GetLocalStorage(main_frame, @"x", &result, &error_message));
  EXPECT_NSEQ(error_message, kLocalStorageErrorMessage);
  EXPECT_NSEQ(nil, result);
}

// Tests that sessionStorage is accessible from JavaScript in all frames
// when the blocking mode is set to allow.
TEST_F(CookieBlockingTest, SessionStorageAllowed) {
  __block bool success = false;
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kAllow,
                                           base::BindOnce(^{
                                             success = true;
                                           }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL("localhost", kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  for (WebFrame* frame :
       web_state()->GetWebFramesManager()->GetAllWebFrames()) {
    NSString* error_message;
    EXPECT_TRUE(
        web::test::SetSessionStorage(frame, @"x", @"value", &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(nil, error_message) << FailureMessage(frame);

    error_message = nil;
    NSString* result;
    EXPECT_TRUE(
        web::test::GetSessionStorage(frame, @"x", &result, &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(nil, error_message) << FailureMessage(frame);
    EXPECT_NSEQ(@"value", result) << FailureMessage(frame);
  }
}

// Tests that sessionStorage is inaccessable from JavaScript in all frames
// when the blocking mode is set to block.
TEST_F(CookieBlockingTest, SessionStorageBlocked) {
  __block bool success = false;
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlock,
                                           base::BindOnce(^{
                                             success = true;
                                           }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL("localhost", kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  for (WebFrame* frame :
       web_state()->GetWebFramesManager()->GetAllWebFrames()) {
    NSString* error_message;
    EXPECT_TRUE(
        web::test::SetSessionStorage(frame, @"x", @"value", &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(error_message, kSessionStorageErrorMessage)
        << FailureMessage(frame);

    error_message = nil;
    NSString* result;
    EXPECT_TRUE(
        web::test::GetSessionStorage(frame, @"x", &result, &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(error_message, kSessionStorageErrorMessage)
        << FailureMessage(frame);
    EXPECT_NSEQ(nil, result) << FailureMessage(frame);
  }
}

// Tests that the sessionStorage override is undeletable via extra JavaScript.
TEST_F(CookieBlockingTest, SessionStorageBlockedUndeletable) {
  __block bool success = false;
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlock,
                                           base::BindOnce(^{
                                             success = true;
                                           }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL("localhost", kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  web_state()->ExecuteJavaScript(u"delete sessionStorage");

  NSString* error_message;
  WebFrame* main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
  EXPECT_TRUE(
      web::test::SetSessionStorage(main_frame, @"x", @"value", &error_message));
  EXPECT_NSEQ(error_message, kSessionStorageErrorMessage);

  error_message = nil;
  NSString* result;
  EXPECT_TRUE(
      web::test::GetSessionStorage(main_frame, @"x", &result, &error_message));
  EXPECT_NSEQ(error_message, kSessionStorageErrorMessage);
  EXPECT_NSEQ(nil, result);
}

// Tests that Cache Storage is accessible from JavaScript in frames
// when the blocking mode is set to allow.
TEST_F(CookieBlockingTest, CacheStorageAllowed) {
  __block bool success = false;
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kAllow,
                                           base::BindOnce(^{
                                             success = true;
                                           }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL("localhost", kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  bool one_frame_succeeded = false;

  for (WebFrame* frame :
       web_state()->GetWebFramesManager()->GetAllWebFrames()) {
    NSString* error_message;
    EXPECT_TRUE(
        web::test::SetCache(frame, web_state(), @"x", @"value", &error_message))
        << FailureMessage(frame);
    if ([error_message isEqualToString:kCacheNotAvailableErrorMessage]) {
      // Sometimes, the Cache API is not available. In these cases, the test
      // shouldn't fail.
      continue;
    }
    EXPECT_NSEQ(nil, error_message) << FailureMessage(frame);

    error_message = nil;
    NSString* result;
    EXPECT_TRUE(
        web::test::GetCache(frame, web_state(), @"x", &result, &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(nil, error_message) << FailureMessage(frame);
    EXPECT_NSEQ(@"value", result) << FailureMessage(frame);
    one_frame_succeeded = true;
  }
  EXPECT_TRUE(one_frame_succeeded);
}

// Tests that Cache Storage is blocked from JavaScript in frames
// when the blocking mode is set to blocked.
TEST_F(CookieBlockingTest, CacheStorageBlocked) {
  __block bool success = false;
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlock,
                                           base::BindOnce(^{
                                             success = true;
                                           }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL("localhost", kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  bool one_frame_succeeded = false;

  for (WebFrame* frame :
       web_state()->GetWebFramesManager()->GetAllWebFrames()) {
    NSString* error_message;
    EXPECT_TRUE(
        web::test::SetCache(frame, web_state(), @"x", @"value", &error_message))
        << FailureMessage(frame);
    if ([error_message isEqualToString:kCacheNotAvailableErrorMessage]) {
      // Sometimes, the Cache API is not available. In these cases, the test
      // shouldn't fail.
      continue;
    }
    EXPECT_NSEQ(kCacheErrorMessage, error_message) << FailureMessage(frame);

    error_message = nil;
    NSString* result;
    EXPECT_TRUE(
        web::test::GetCache(frame, web_state(), @"x", &result, &error_message))
        << FailureMessage(frame);
    EXPECT_NSEQ(kCacheErrorMessage, error_message) << FailureMessage(frame);
    EXPECT_NSEQ(nil, result) << FailureMessage(frame);
    one_frame_succeeded = true;
  }
  EXPECT_TRUE(one_frame_succeeded);
}

// Tests that IndexedDB is accessible from JavaScript in the main frame
// when the blocking mode is set to allow.
TEST_F(CookieBlockingTest, IndexedDBAllowed) {
  __block bool success = false;
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kAllow,
                                           base::BindOnce(^{
                                             success = true;
                                           }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL("localhost", kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  // Only test in the main frame because WebKit already disallows indexedDB in
  // cross-origin iframes.
  WebFrame* main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
  NSString* error_message;
  EXPECT_TRUE(web::test::SetIndexedDB(main_frame, web_state(), @"x", @"value",
                                      &error_message))
      << FailureMessage(main_frame);
  EXPECT_NSEQ(nil, error_message) << FailureMessage(main_frame);

  error_message = nil;
  NSString* result;
  EXPECT_TRUE(web::test::GetIndexedDB(main_frame, web_state(), @"x", &result,
                                      &error_message))
      << FailureMessage(main_frame);
  EXPECT_NSEQ(nil, error_message) << FailureMessage(main_frame);
  EXPECT_NSEQ(@"value", result) << FailureMessage(main_frame);
}

// Tests that IndexedDB is blocked from JavaScript in the main frame
// when the blocking mode is set to blocked.
TEST_F(CookieBlockingTest, IndexedDBBlocked) {
  __block bool success = false;
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlock,
                                           base::BindOnce(^{
                                             success = true;
                                           }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  // Use arbitrary third party url for iframe.
  GURL iframe_url = third_party_server_.GetURL("localhost", kIFrameUrl);
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));

  // Only test in the main frame because WebKit already disallows indexedDB in
  // cross-origin iframes.
  WebFrame* main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
  NSString* error_message;
  EXPECT_TRUE(web::test::SetIndexedDB(main_frame, web_state(), @"x", @"value",
                                      &error_message))
      << FailureMessage(main_frame);
  EXPECT_NSEQ(kIndexedDBErrorMessage, error_message)
      << FailureMessage(main_frame);

  error_message = nil;
  NSString* result;
  EXPECT_TRUE(web::test::GetIndexedDB(main_frame, web_state(), @"x", &result,
                                      &error_message))
      << FailureMessage(main_frame);
  EXPECT_NSEQ(kIndexedDBErrorMessage, error_message)
      << FailureMessage(main_frame);
  EXPECT_NSEQ(nil, result) << FailureMessage(main_frame);
}

// Tests that the cookies sent in HTTP headers are allowed.
TEST_F(CookieBlockingTest, RequestCookiesAllowed) {
  __block bool success = false;
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kAllow,
                                           base::BindOnce(^{
                                             success = true;
                                           }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  // Check that page doesn't send a=b cookie initially.
  test::LoadUrl(web_state(), server_.GetURL("/get-cookies"));
  EXPECT_TRUE(test::WaitForPageToFinishLoading(web_state()));
  EXPECT_EQ(std::string::npos, latest_get_cookies_.find("a=b"));

  // Set cookie.
  test::LoadUrl(web_state(), server_.GetURL("/set-cookies"));
  EXPECT_TRUE(test::WaitForPageToFinishLoading(web_state()));

  // Check that page does send a=b cookie.
  test::LoadUrl(web_state(), server_.GetURL("/get-cookies"));
  EXPECT_TRUE(test::WaitForPageToFinishLoading(web_state()));

  EXPECT_NE(std::string::npos, latest_get_cookies_.find("a=b"));
}

// Tests that the cookies sent in HTTP headers are blocked.
TEST_F(CookieBlockingTest, RequestCookiesBlocked) {
  __block bool success = false;
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlock,
                                           base::BindOnce(^{
                                             success = true;
                                           }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  // Check that page doesn't send a=b cookie initially.
  test::LoadUrl(web_state(), server_.GetURL("/get-cookies"));
  EXPECT_TRUE(test::WaitForPageToFinishLoading(web_state()));
  EXPECT_EQ(std::string::npos, latest_get_cookies_.find("a=b"));

  // Set cookie.
  test::LoadUrl(web_state(), server_.GetURL("/set-cookies"));
  EXPECT_TRUE(test::WaitForPageToFinishLoading(web_state()));

  // Check that page stil doesn't send a=b cookie.
  test::LoadUrl(web_state(), server_.GetURL("/get-cookies"));
  EXPECT_TRUE(test::WaitForPageToFinishLoading(web_state()));
  EXPECT_EQ(std::string::npos, latest_get_cookies_.find("a=b"));
}

// Tests that the cookies sent in HTTP headers are blocked.
TEST_F(CookieBlockingTest, RequestCookiesBlockedThirdParty) {
  __block bool success = false;
  GetBrowserState()->SetCookieBlockingMode(CookieBlockingMode::kBlockThirdParty,
                                           base::BindOnce(^{
                                             success = true;
                                           }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  // Check that page doesn't send a=b cookie.
  test::LoadUrl(web_state(),
                third_party_server_.GetURL("localhost", "/get-cookies"));
  EXPECT_TRUE(test::WaitForPageToFinishLoading(web_state()));
  EXPECT_EQ(std::string::npos, latest_get_cookies_.find("a=b"));

  // Set cookie.
  test::LoadUrl(web_state(),
                third_party_server_.GetURL("localhost", "/set-cookies"));
  EXPECT_TRUE(test::WaitForPageToFinishLoading(web_state()));

  // Check that page does send a=b cookie in a first-party context.
  test::LoadUrl(web_state(),
                third_party_server_.GetURL("localhost", "/get-cookies"));
  EXPECT_TRUE(test::WaitForPageToFinishLoading(web_state()));
  EXPECT_NE(std::string::npos, latest_get_cookies_.find("a=b"));

  // Load page in third-party context and check that page doesn't send cookie.
  GURL iframe_url = third_party_server_.GetURL("localhost", "/get-cookies");
  std::string url_spec = kPageUrl + net::EscapeQueryParamValue(
                                        iframe_url.spec(), /*use_plus=*/true);
  test::LoadUrl(web_state(), server_.GetURL(url_spec));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));
  EXPECT_EQ(std::string::npos, latest_get_cookies_.find("a=b"));
}

}  // namespace web
