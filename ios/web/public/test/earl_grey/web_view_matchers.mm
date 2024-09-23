// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/earl_grey/web_view_matchers.h"

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"

// TODO(crbug.com/41340619): Remove this class, after LoadImage() is removed.
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

}  // namespace web
