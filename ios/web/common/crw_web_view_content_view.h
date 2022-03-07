// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_CRW_WEB_VIEW_CONTENT_VIEW_H_
#define IOS_WEB_COMMON_CRW_WEB_VIEW_CONTENT_VIEW_H_

#import <WebKit/WebKit.h>

#import "ios/web/common/crw_content_view.h"
#import "ios/web/common/crw_viewport_adjustment.h"

// Wraps a web vew in a CRWContentView.
@interface CRWWebViewContentView : CRWContentView <CRWViewportAdjustment>

// The webView passed to |-initWithWebView|.
@property(nonatomic, strong, readonly) UIView* webView;

#if defined(__IPHONE_15_4) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_15_4
// The fullscreen state of this view
@property(nonatomic, readonly)
    WKFullscreenState fullscreenState API_AVAILABLE(ios(15));

// Initializes the CRWWebViewContentView to display |webView| and passes state
// of fullscreen mode. This should become the default constructor when we
// start building with the 15.4 SDK and pass in a default value of
// WKFullscreenState (e.g., WKFullscreenStateNotInFullscreen). Additionally,
// code in initWithWebView:scrollView should be moved into this constructor at
// that time.
- (instancetype)initWithWebView:(UIView*)webView
                     scrollView:(UIScrollView*)scrollView
                fullscreenState:(WKFullscreenState)fullscreenState
    API_AVAILABLE(ios(15));
#endif  // defined(__IPHONE_15_4)

// Initializes the CRWWebViewContentView to display |webView|.
- (instancetype)initWithWebView:(UIView*)webView
                     scrollView:(UIScrollView*)scrollView
    NS_DESIGNATED_INITIALIZER;

// Available only for testing.
- (instancetype)initForTesting NS_DESIGNATED_INITIALIZER;

// CRWWebViewContentViews should be initialized via |-initWithWebView:
// scrollView:|.
- (instancetype)initWithCoder:(NSCoder*)decoder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

#if defined(__IPHONE_15_4) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_15_4
- (void)updateFullscreenState:(WKFullscreenState)fullscreenState
    API_AVAILABLE(ios(15));
#endif  // defined(__IPHONE_15_4)

@end

#endif  // IOS_WEB_COMMON_CRW_WEB_VIEW_CONTENT_VIEW_H_
