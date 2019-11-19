// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_VIEW_CONTENT_VIEW_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_VIEW_CONTENT_VIEW_H_

#import "ios/web/common/crw_web_view_content_view.h"

// A test version of CRWWebViewContentView.
@interface TestWebViewContentView : CRWWebViewContentView

// Initializes the TestWebViewContentView.  Since |webView| and |scrollView| may
// be mock objects, they will not be added as subviews.
- (instancetype)initWithMockWebView:(id)webView scrollView:(id)scrollView;

// TestWebViewContentViews should be initialized via |-initWithMockWebView:
// scrollView:|.
- (instancetype)initWithWebView:(UIView*)webView
                     scrollView:(UIScrollView*)scrollView NS_UNAVAILABLE;
@end

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_VIEW_CONTENT_VIEW_H_
