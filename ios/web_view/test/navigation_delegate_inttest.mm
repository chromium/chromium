// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <ChromeWebView/ChromeWebView.h>
#import <Foundation/Foundation.h>

#import "ios/web_view/test/web_view_inttest_base.h"
#import "ios/web_view/test/web_view_test_util.h"
#import "net/base/apple/url_conversions.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "url/gurl.h"

namespace ios_web_view {

// Tests CWVNavigationDelegate.
class NavigationDelegateTest : public ios_web_view::WebViewInttestBase {
 public:
  NavigationDelegateTest()
      : mock_delegate_(
            OCMStrictProtocolMock(@protocol(CWVNavigationDelegate))) {
    [(id)mock_delegate_ setExpectationOrderMatters:YES];
    web_view_.navigationDelegate = mock_delegate_;
  }

  void SetUp() override {
    ios_web_view::WebViewInttestBase::SetUp();
    ASSERT_TRUE(test_server_->Start());
  }

  NSURL* GetEchoURL() {
    return net::NSURLWithGURL(test_server_->GetURL("/echo"));
  }

  NSURL* GetCloseSocketURL() {
    return net::NSURLWithGURL(test_server_->GetURL("/close-socket"));
  }

  id NavigationActionArg(NSURL* url, CWVNavigationType navigation_type) {
    return [OCMArg checkWithBlock:^(CWVNavigationAction* navigation_action) {
      return [navigation_action.request.URL isEqual:url] &&
             navigation_action.navigationType == navigation_type;
    }];
  }

  id NavigationResponseArg(NSURL* url, bool for_main_frame) {
    return
        [OCMArg checkWithBlock:^(CWVNavigationResponse* navigation_response) {
          return [navigation_response.response.URL isEqual:url] &&
                 navigation_response.forMainFrame == for_main_frame;
        }];
  }

  id<CWVNavigationDelegate> mock_delegate_;
};

// Tests that expected delegate methods are called for a successful request.
TEST_F(NavigationDelegateTest, RequestSucceeds) {
  // A request made with -loadRequest: has type CWVNavigationTypeTyped.
  OCMExpect([mock_delegate_ webView:web_view_
                decidePolicyForNavigationAction:NavigationActionArg(
                                                    GetEchoURL(),
                                                    CWVNavigationTypeTyped)
                                decisionHandler:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        __weak void (^decisionHandler)(CWVNavigationActionPolicy);
        [invocation getArgument:&decisionHandler atIndex:4];
        if (decisionHandler) {
          decisionHandler(CWVNavigationActionPolicyAllow);
        }
      });
  OCMExpect([mock_delegate_ webViewDidStartProvisionalNavigation:web_view_]);
  OCMExpect([mock_delegate_ webViewDidStartNavigation:web_view_]);
  OCMExpect([mock_delegate_ webView:web_view_
                decidePolicyForNavigationResponse:NavigationResponseArg(
                                                      GetEchoURL(), YES)
                                  decisionHandler:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        __weak void (^decisionHandler)(CWVNavigationResponsePolicy);
        [invocation getArgument:&decisionHandler atIndex:4];
        if (decisionHandler) {
          decisionHandler(CWVNavigationResponsePolicyAllow);
        }
      });
  OCMExpect([mock_delegate_ webViewDidCommitNavigation:web_view_]);
  OCMExpect([mock_delegate_ webViewDidFinishNavigation:web_view_]);

  ASSERT_TRUE(test::LoadUrl(web_view_, GetEchoURL()));
  [(id)mock_delegate_ verify];
}

// Tests that expected delegate methods are called for a failed request.
TEST_F(NavigationDelegateTest, RequestFails) {
  OCMExpect([mock_delegate_ webView:web_view_
                decidePolicyForNavigationAction:NavigationActionArg(
                                                    GetCloseSocketURL(),
                                                    CWVNavigationTypeTyped)
                                decisionHandler:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        __weak void (^decisionHandler)(CWVNavigationActionPolicy);
        [invocation getArgument:&decisionHandler atIndex:4];
        if (decisionHandler) {
          decisionHandler(CWVNavigationActionPolicyAllow);
        }
      });
  OCMExpect([mock_delegate_ webViewDidStartProvisionalNavigation:web_view_]);
  OCMExpect([mock_delegate_ webViewDidStartNavigation:web_view_]);
  OCMExpect([mock_delegate_ webViewDidCommitNavigation:web_view_]);
  OCMExpect([mock_delegate_ webView:web_view_
         didFailNavigationWithError:[OCMArg any]]);

  ASSERT_TRUE(test::LoadUrl(web_view_, GetCloseSocketURL()));
  [(id)mock_delegate_ verify];

  // Wait for the error text to be injected to make sure that the JavaScript has
  // been correctly injected.
  ASSERT_TRUE(ios_web_view::test::WaitForWebViewContainingTextOrTimeout(
      web_view_, @"The network connection was lost."));
}

// Tests that a request is canceled and no further delegate methods are called
// when -shouldStartLoadWithRequest:navigationType: returns NO.
TEST_F(NavigationDelegateTest, CancelRequest) {
  OCMExpect([mock_delegate_ webView:web_view_
                decidePolicyForNavigationAction:NavigationActionArg(
                                                    GetEchoURL(),
                                                    CWVNavigationTypeTyped)
                                decisionHandler:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        __weak void (^decisionHandler)(CWVNavigationActionPolicy);
        [invocation getArgument:&decisionHandler atIndex:4];
        if (decisionHandler) {
          decisionHandler(CWVNavigationActionPolicyCancel);
        }
      });

  ASSERT_TRUE(test::LoadUrl(web_view_, GetEchoURL()));
  [(id)mock_delegate_ verify];
}

// Tests that a response is canceled and no further delegate methods are called
// when -shouldContinueLoadWithResponse:forMainFrame: returns NO.
TEST_F(NavigationDelegateTest, CancelResponse) {
  OCMExpect([mock_delegate_ webView:web_view_
                decidePolicyForNavigationAction:NavigationActionArg(
                                                    GetEchoURL(),
                                                    CWVNavigationTypeTyped)
                                decisionHandler:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        __weak void (^decisionHandler)(CWVNavigationActionPolicy);
        [invocation getArgument:&decisionHandler atIndex:4];
        if (decisionHandler) {
          decisionHandler(CWVNavigationActionPolicyAllow);
        }
      });
  OCMExpect([mock_delegate_ webViewDidStartProvisionalNavigation:web_view_]);
  OCMExpect([mock_delegate_ webViewDidStartNavigation:web_view_]);
  OCMExpect([mock_delegate_ webView:web_view_
                decidePolicyForNavigationResponse:NavigationResponseArg(
                                                      GetEchoURL(), YES)
                                  decisionHandler:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        __weak void (^decisionHandler)(CWVNavigationResponsePolicy);
        [invocation getArgument:&decisionHandler atIndex:4];
        if (decisionHandler) {
          decisionHandler(CWVNavigationResponsePolicyCancel);
        }
      });

  ASSERT_TRUE(test::LoadUrl(web_view_, GetEchoURL()));
  [(id)mock_delegate_ verify];
}

// Tests that same document navigations do not trigger delegate methods.
TEST_F(NavigationDelegateTest, SameDocumentNavigations) {
  // A request made with -loadRequest: has type CWVNavigationTypeTyped.
  OCMExpect([mock_delegate_ webView:web_view_
                decidePolicyForNavigationAction:NavigationActionArg(
                                                    GetEchoURL(),
                                                    CWVNavigationTypeTyped)
                                decisionHandler:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        __weak void (^decisionHandler)(CWVNavigationActionPolicy);
        [invocation getArgument:&decisionHandler atIndex:4];
        if (decisionHandler) {
          decisionHandler(CWVNavigationActionPolicyAllow);
        }
      });
  OCMExpect([mock_delegate_ webViewDidStartProvisionalNavigation:web_view_]);
  OCMExpect([mock_delegate_ webViewDidStartNavigation:web_view_]);
  OCMExpect([mock_delegate_ webView:web_view_
                decidePolicyForNavigationResponse:NavigationResponseArg(
                                                      GetEchoURL(), YES)
                                  decisionHandler:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        __weak void (^decisionHandler)(CWVNavigationResponsePolicy);
        [invocation getArgument:&decisionHandler atIndex:4];
        if (decisionHandler) {
          decisionHandler(CWVNavigationResponsePolicyAllow);
        }
      });
  OCMExpect([mock_delegate_ webViewDidCommitNavigation:web_view_]);
  OCMExpect([mock_delegate_ webViewDidFinishNavigation:web_view_]);

  ASSERT_TRUE(test::LoadUrl(web_view_, GetEchoURL()));

  [(id)mock_delegate_ verify];

  // Same document navigations should not trigger the delegate methods.
  NSError* error = nil;
  ASSERT_NSEQ(nil, test::EvaluateJavaScript(
                       web_view_, @"history.pushState({}, \"\");", &error));
  EXPECT_FALSE(error);

  [(id)mock_delegate_ verify];
}

}  // namespace ios_web_view
