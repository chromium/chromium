// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/earl_grey/web_view_matchers.h"

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#import "ios/web/security/web_interstitial_impl.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TODO(crbug.com/757982): Remove this class, after LoadImage() is removed.
// A helper delegate class that allows downloading responses with invalid
// SSL certs.
@interface TestURLSessionDelegateDeprecated : NSObject<NSURLSessionDelegate>
@end

@implementation TestURLSessionDelegateDeprecated

- (void)URLSession:(NSURLSession*)session
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                  NSURLCredential*))completionHandler {
  SecTrustRef serverTrust = challenge.protectionSpace.serverTrust;
  completionHandler(NSURLSessionAuthChallengeUseCredential,
                    [NSURLCredential credentialForTrust:serverTrust]);
}

@end

namespace web {

id<GREYMatcher> WebViewInWebState(WebState* web_state) {
  GREYMatchesBlock matches = ^BOOL(UIView* view) {
    return [view isKindOfClass:[WKWebView class]] &&
           [view isDescendantOfView:web_state->GetView()];
  };

  GREYDescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"web view in web state"];
  };

  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

id<GREYMatcher> WebViewScrollView(WebState* web_state) {
  GREYMatchesBlock matches = ^BOOL(UIView* view) {
    return [view isKindOfClass:[UIScrollView class]] &&
           [view.superview isKindOfClass:[WKWebView class]] &&
           [view isDescendantOfView:web_state->GetView()];
  };

  GREYDescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"web view scroll view"];
  };

  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

id<GREYMatcher> Interstitial(WebState* web_state) {
  GREYMatchesBlock matches = ^BOOL(WKWebView* view) {
    web::WebInterstitialImpl* interstitial =
        static_cast<web::WebInterstitialImpl*>(web_state->GetWebInterstitial());
    return interstitial &&
           [view isDescendantOfView:interstitial->GetContentView()];
  };

  GREYDescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"interstitial displayed"];
  };

  return grey_allOf(
      WebViewInWebState(web_state),
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe],
      nil);
}

}  // namespace web
