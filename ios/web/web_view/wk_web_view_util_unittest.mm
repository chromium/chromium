// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_view/wk_web_view_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WKPreferences (Private)

@property(nonatomic,
          getter=_isSafeBrowsingEnabled,
          setter=_setSafeBrowsingEnabled:) BOOL _safeBrowsingEnabled;

@end

@interface WKWebView (Private)

- (void)_showSafeBrowsingWarningWithURL:(NSURL*)url
                                  title:(NSString*)title
                                warning:(NSString*)warning
                                details:(NSAttributedString*)details
                      completionHandler:(void (^)(BOOL))completionHandler;
@end

class WKWebViewUtilTest : public PlatformTest {};

// Tests that IsSafeBrowsingWarningDisplayedInWebView returns true when safe
// browsing warning is displayed in WKWebView.
TEST_F(WKWebViewUtilTest, TestIsSafeBrowsingWarningDisplayedInWebView) {
  if (@available(iOS 12.2, *)) {
    UIViewController* controller = [[UIViewController alloc] init];
    UIApplication.sharedApplication.keyWindow.rootViewController = controller;
    WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
    WKWebView* web_view = [[WKWebView alloc] initWithFrame:CGRectZero
                                             configuration:config];
    [controller.view addSubview:web_view];

    // Use private API of WKPreferences to enable safe browsing warning.
    [config.preferences _setSafeBrowsingEnabled:YES];

    // Use private API of WKWebView to show safe browsing warning.
    [web_view _showSafeBrowsingWarningWithURL:nil
                                        title:nil
                                      warning:nil
                                      details:nil
                            completionHandler:nil];

    EXPECT_TRUE(web::IsSafeBrowsingWarningDisplayedInWebView(web_view));
  }
}
