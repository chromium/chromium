// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_CRW_WEB_VIEW_CONTENT_VIEW_H_
#define IOS_WEB_COMMON_CRW_WEB_VIEW_CONTENT_VIEW_H_

#import <WebKit/WebKit.h>

#import "ios/web/common/crw_content_view.h"
#import "ios/web/common/crw_viewport_adjustment.h"

// Wraps a web vew in a CRWContentView.
@interface CRWWebViewContentView : CRWContentView <CRWViewportAdjustment>

// The webView passed to `-initWithWebView`.
@property(nonatomic, strong, readonly) UIView* webView;

// The fullscreen state of this view
@property(nonatomic, readonly) CrFullscreenState fullscreenState;

// Initializes the CRWWebViewContentView to display `webView` and passes state
// of fullscreen mode. This should pass in a default value of
// CrFullscreenState (e.g., kNotInFullScreen).
- (instancetype)initWithWebView:(UIView*)webView
                     scrollView:(UIScrollView*)scrollView
                fullscreenState:(CrFullscreenState)fullscreenState
    NS_DESIGNATED_INITIALIZER;

// Available only for testing.
- (instancetype)initForTesting NS_DESIGNATED_INITIALIZER;

// CRWWebViewContentViews should be initialized via `-initWithWebView:
// scrollView:`.
- (instancetype)initWithCoder:(NSCoder*)decoder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

- (void)updateFullscreenState:(CrFullscreenState)fullscreenState;

@end

#endif  // IOS_WEB_COMMON_CRW_WEB_VIEW_CONTENT_VIEW_H_
