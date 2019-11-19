// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_CRW_WEB_VIEW_CONTENT_VIEW_H_
#define IOS_WEB_COMMON_CRW_WEB_VIEW_CONTENT_VIEW_H_

#import "ios/web/common/crw_content_view.h"

// Wraps a web vew in a CRWContentView.
@interface CRWWebViewContentView : CRWContentView

// The webView passed to |-initWithWebView|.
@property(nonatomic, strong, readonly) UIView* webView;

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

@end

#endif  // IOS_WEB_COMMON_CRW_WEB_VIEW_CONTENT_VIEW_H_
