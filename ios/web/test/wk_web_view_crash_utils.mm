// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/wk_web_view_crash_utils.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import "base/check.h"
#import "ios/web/common/web_view_creation_util.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "third_party/ocmock/OCMock/NSInvocation+OCMAdditions.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

// Returns an OCMocked WKWebView whose `evaluateJavaScript:completionHandler:`
// method has been mocked to execute `block` instead. `block` cannot be nil.
WKWebView* BuildMockWKWebViewWithStubbedJSEvalFunction(
    void (^block)(NSInvocation*)) {
  DCHECK(block);
  web::FakeBrowserState browser_state;
  WKWebView* web_view = web::BuildWKWebView(CGRectZero, &browser_state);
  id mock_web_view = [OCMockObject partialMockForObject:web_view];
  [[[mock_web_view stub] andDo:^void(NSInvocation* invocation) {
    block(invocation);
  }] evaluateJavaScript:OCMOCK_ANY completionHandler:OCMOCK_ANY];
  return mock_web_view;
}

}  // namespace

namespace web {

void SimulateWKWebViewCrash(WKWebView* webView) {
  SEL selector = @selector(webViewWebContentProcessDidTerminate:);
  if ([webView.navigationDelegate respondsToSelector:selector]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    [webView.navigationDelegate performSelector:selector withObject:webView];
#pragma clang diagnostic pop
  }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundeclared-selector"
  [webView performSelector:@selector(_processDidExit)];
#pragma clang diagnostic pop
}

WKWebView* BuildTerminatedWKWebView() {
  id fail = ^void(NSInvocation* invocation) {
      // Always fails with WKErrorWebContentProcessTerminated error.
      NSError* error =
          [NSError errorWithDomain:WKErrorDomain
                              code:WKErrorWebContentProcessTerminated
                          userInfo:nil];

      void (^completionHandler)(id, NSError*) =
          [invocation getArgumentAtIndexAsObject:3];
      completionHandler(nil, error);
  };
  return BuildMockWKWebViewWithStubbedJSEvalFunction(fail);
}

WKWebView* BuildHealthyWKWebView() {
  id succeed = ^void(NSInvocation* invocation) {
      void (^completionHandler)(id, NSError*) =
          [invocation getArgumentAtIndexAsObject:3];
      // Always succceeds with nil result.
      completionHandler(nil, nil);
  };
  return BuildMockWKWebViewWithStubbedJSEvalFunction(succeed);
}

}  // namespace web
