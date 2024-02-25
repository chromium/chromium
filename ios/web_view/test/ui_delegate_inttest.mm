// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <ChromeWebView/ChromeWebView.h>
#import <Foundation/Foundation.h>

#import "base/test/ios/wait_util.h"
#import "ios/web_view/test/web_view_inttest_base.h"
#import "ios/web_view/test/web_view_test_util.h"
#import "net/base/apple/url_conversions.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "url/gurl.h"

using base::test::ios::kWaitForUIElementTimeout;

namespace ios_web_view {

// Tests CWVUIDelegate.
class UIDelegateTest : public ios_web_view::WebViewInttestBase {
 public:
  UIDelegateTest() : mock_delegate_(OCMProtocolMock(@protocol(CWVUIDelegate))) {
    web_view_.UIDelegate = mock_delegate_;
  }

  void SetUp() override {
    ios_web_view::WebViewInttestBase::SetUp();
    ASSERT_TRUE(test_server_->Start());
  }

  NSURL* GetEchoURL() {
    return net::NSURLWithGURL(test_server_->GetURL("/echo"));
  }

  id<CWVUIDelegate> mock_delegate_;
};

// Tests -webView:createWebViewWithConfiguration:forNavigationAction:
TEST_F(UIDelegateTest, CreateWebView) {
  id expected_navigation_action =
      [OCMArg checkWithBlock:^(CWVNavigationAction* action) {
        return
            [action.request.URL.absoluteString isEqual:@"http://example.com/"];
      }];
  OCMExpect([mock_delegate_ webView:web_view_
      createWebViewWithConfiguration:web_view_.configuration
                 forNavigationAction:expected_navigation_action]);

  ASSERT_TRUE(test::LoadUrl(web_view_, GetEchoURL()));

  NSError* error = nil;
  EXPECT_NE(nil,
            test::EvaluateJavaScript(
                web_view_, @"typeof open('http://example.com/') === 'object'",
                &error));
  EXPECT_FALSE(error);

  [(id)mock_delegate_ verify];
}

// Tests -webView:runJavaScriptAlertPanelWithMessage:pageURL:completionHandler:
TEST_F(UIDelegateTest, RunJavaScriptAlertPanel) {
  id mock_completion_handler =
      [OCMArg checkWithBlock:^(void (^completionHandler)(void)) {
        completionHandler();
        return YES;
      }];

  OCMExpect([mock_delegate_ webView:web_view_
      runJavaScriptAlertPanelWithMessage:@"message"
                                 pageURL:GetEchoURL()
                       completionHandler:mock_completion_handler]);

  ASSERT_TRUE(test::LoadUrl(web_view_, GetEchoURL()));
  NSError* error = nil;
  test::EvaluateJavaScript(web_view_, @"alert('message')", &error);
  EXPECT_FALSE(error);

  [(id)mock_delegate_ verify];
}

// Tests
// -webView:runJavaScriptConfirmPanelWithMessage:pageURL:completionHandler:
TEST_F(UIDelegateTest, RunJavaScriptConfirmPanel) {
  id mock_completion_handler =
      [OCMArg checkWithBlock:^(void (^completionHandler)(BOOL)) {
        completionHandler(YES);
        return YES;
      }];

  OCMExpect([mock_delegate_ webView:web_view_
      runJavaScriptConfirmPanelWithMessage:@"message"
                                   pageURL:GetEchoURL()
                         completionHandler:mock_completion_handler]);

  ASSERT_TRUE(test::LoadUrl(web_view_, GetEchoURL()));
  NSError* error = nil;
  EXPECT_TRUE([test::EvaluateJavaScript(web_view_, @"confirm('message')",
                                        &error) boolValue]);
  EXPECT_FALSE(error);

  [(id)mock_delegate_ verify];
}

// Tests
// -webView:runJavaScriptTextInputPanelWithPrompt:pageURL:completionHandler:
TEST_F(UIDelegateTest, RunJavaScriptTextInputPanel) {
  id mock_completion_handler =
      [OCMArg checkWithBlock:^(void (^completionHandler)(NSString*)) {
        completionHandler(@"input");
        return YES;
      }];

  OCMExpect([mock_delegate_ webView:web_view_
      runJavaScriptTextInputPanelWithPrompt:@"prompt"
                                defaultText:@"default"
                                    pageURL:GetEchoURL()
                          completionHandler:mock_completion_handler]);

  ASSERT_TRUE(test::LoadUrl(web_view_, GetEchoURL()));
  NSError* error = nil;
  EXPECT_NSEQ(@"input", test::EvaluateJavaScript(
                            web_view_, @"prompt('prompt', 'default')", &error));
  EXPECT_FALSE(error);

  [(id)mock_delegate_ verify];
}

// Tests -webView:didLoadFavicons:
TEST_F(UIDelegateTest, DidLoadFavicons) {
  NSURL* page_url = net::NSURLWithGURL(GetUrlForPageWithHtml(R"(
    <!DOCTYPE html>
    <html>
      <head>
        <link rel="icon" href="/testfavicon.png">
      </head>
      <body></body>
    </html>
  )"));

  // This file does not exist, but it doesn't matter for this test.
  NSURL* favicon_url =
      net::NSURLWithGURL(test_server_->GetURL("/testfavicon.png"));

  __block NSArray<CWVFavicon*>* favicons = nil;
  id favicons_arg = [OCMArg checkWithBlock:^(NSArray<CWVFavicon*>* value) {
    favicons = value;
    return YES;
  }];
  OCMExpect([mock_delegate_ webView:web_view_ didLoadFavicons:favicons_arg]);

  ASSERT_TRUE(test::LoadUrl(web_view_, page_url));
  [(id)mock_delegate_ verifyWithDelay:kWaitForUIElementTimeout.InSecondsF()];

  ASSERT_EQ(1u, favicons.count);
  EXPECT_EQ(CWVFaviconTypeFavicon, favicons[0].type);
  EXPECT_NSEQ(favicon_url, favicons[0].URL);
}

}  // namespace ios_web_view
