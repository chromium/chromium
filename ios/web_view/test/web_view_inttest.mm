// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <ChromeWebView/ChromeWebView.h>
#import <Foundation/Foundation.h>

#include "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "ios/web_view/test/web_view_inttest_base.h"
#import "ios/web_view/test/web_view_test_util.h"
#import "net/base/apple/url_conversions.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

namespace ios_web_view {

// Tests public methods in CWVWebView.
//
// Note that some methods are covered by other tests in this directory.
class WebViewTest : public ios_web_view::WebViewInttestBase {
 public:
  WebViewTest() {
    test_server_->RegisterRequestHandler(base::BindRepeating(
        &WebViewTest::CaptureRequestHandler, base::Unretained(this)));
  }

  void SetUp() override {
    ios_web_view::WebViewInttestBase::SetUp();
    CWVWebView.customUserAgent = nil;
  }

  void TearDown() override {
    ios_web_view::WebViewInttestBase::TearDown();
    CWVWebView.customUserAgent = nil;
  }

  std::unique_ptr<net::test_server::HttpResponse> CaptureRequestHandler(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/CaptureRequest") {
      last_request_ = std::make_unique<net::test_server::HttpRequest>(request);
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_content(
          "<html><body>CaptureRequestHTML</body></html>");
      return std::move(http_response);
    }
    return nullptr;
  }

  std::unique_ptr<net::test_server::HttpRequest> last_request_;
};

// Tests +[CWVWebView customUserAgent].
TEST_F(WebViewTest, CustomUserAgent) {
  ASSERT_TRUE(test_server_->Start());

  CWVWebView.customUserAgent = @"FooCustomUserAgent";
  ASSERT_NSEQ(@"FooCustomUserAgent", CWVWebView.customUserAgent);

  // Cannot use existing |web_view_| here because the change above may only
  // affect web views created after the change.
  CWVWebView* web_view = test::CreateWebView();
  GURL url = test_server_->GetURL("/CaptureRequest");
  ASSERT_TRUE(test::LoadUrl(web_view, net::NSURLWithGURL(url)));

  // Investigates the HTTP headers captured by CaptureRequestHandler(), and
  // tests that they include User-Agent HTTP header with the specified product
  // name. /echoheader?User-Agent provided by EmbeddedTestServer cannot be used
  // here because it returns content with type text/plain, but we cannot extract
  // the content using test::WaitForWebViewContainingTextOrTimeout() because
  // JavaScript cannot be executed on text/plain content.
  ASSERT_NE(nullptr, last_request_.get());
  auto user_agent_it = last_request_->headers.find("User-Agent");
  ASSERT_NE(last_request_->headers.end(), user_agent_it);
  EXPECT_EQ("FooCustomUserAgent", user_agent_it->second);
}

// Tests +[CWVWebView setUserAgentProduct] and +[CWVWebView userAgentProduct].
TEST_F(WebViewTest, UserAgentProduct) {
  ASSERT_TRUE(test_server_->Start());

  [CWVWebView setUserAgentProduct:@"MyUserAgentProduct"];
  ASSERT_NSEQ(@"MyUserAgentProduct", [CWVWebView userAgentProduct]);

  // Cannot use existing |web_view_| here because the change above may only
  // affect web views created after the change.
  CWVWebView* web_view = test::CreateWebView();
  GURL url = test_server_->GetURL("/CaptureRequest");
  ASSERT_TRUE(test::LoadUrl(web_view, net::NSURLWithGURL(url)));

  // Investigates the HTTP headers captured by CaptureRequestHandler(), and
  // tests that they include User-Agent HTTP header with the specified product
  // name. /echoheader?User-Agent provided by EmbeddedTestServer cannot be used
  // here because it returns content with type text/plain, but we cannot extract
  // the content using test::WaitForWebViewContainingTextOrTimeout() because
  // JavaScript cannot be executed on text/plain content.
  ASSERT_NE(nullptr, last_request_.get());
  auto user_agent_it = last_request_->headers.find("User-Agent");
  ASSERT_NE(last_request_->headers.end(), user_agent_it);
  EXPECT_NE(std::string::npos,
            user_agent_it->second.find("MyUserAgentProduct"));
}

// Tests -[CWVWebView loadRequest:].
TEST_F(WebViewTest, LoadRequest) {
  ASSERT_TRUE(test_server_->Start());
  GURL url = GetUrlForPageWithTitleAndBody("Title", "Body");
  ASSERT_TRUE(test::LoadUrl(web_view_, net::NSURLWithGURL(url)));
  EXPECT_TRUE(test::WaitForWebViewContainingTextOrTimeout(web_view_, @"Body"));
}

// Tests -[CWVWebView reload].
TEST_F(WebViewTest, Reload) {
  ASSERT_TRUE(test_server_->Start());
  GURL url = test_server_->GetURL("/CaptureRequest");
  ASSERT_TRUE(test::LoadUrl(web_view_, net::NSURLWithGURL(url)));

  last_request_.reset(nullptr);
  [web_view_ reload];
  // Tests that a request has been sent again.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return last_request_.get() != nullptr;
  }));
  EXPECT_TRUE(test::WaitForWebViewContainingTextOrTimeout(
      web_view_, @"CaptureRequestHTML"));
}

// Tests -[CWVWebView evaluateJavaScript:completionHandler:].
TEST_F(WebViewTest, EvaluateJavaScript) {
  ASSERT_TRUE(test_server_->Start());
  GURL url = GetUrlForPageWithTitleAndBody("Title", "Body");
  ASSERT_TRUE(test::LoadUrl(web_view_, net::NSURLWithGURL(url)));

  NSError* error;
  EXPECT_NSEQ(@"Body", test::EvaluateJavaScript(
                           web_view_, @"document.body.textContent", &error));
  EXPECT_FALSE(error);

  // Calls a function which doesn't exist.
  test::EvaluateJavaScript(web_view_, @"hoge()", &error);
  EXPECT_TRUE(error);
}

}  // namespace ios_web_view
