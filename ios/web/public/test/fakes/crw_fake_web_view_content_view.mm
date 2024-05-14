// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/crw_fake_web_view_content_view.h"

#import "base/check.h"
#import "base/notreached.h"

@interface CRWFakeWebViewContentView () {
  id _mockWebView;
  id _mockScrollView;
}

@end

@implementation CRWFakeWebViewContentView

- (instancetype)initWithMockWebView:(id)webView scrollView:(id)scrollView {
  self = [super initForTesting];
  if (self) {
    DCHECK(webView);
    DCHECK(scrollView);
    _mockWebView = webView;
    _mockScrollView = scrollView;
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder*)decoder {
  NOTREACHED_IN_MIGRATION();
  return nil;
}

- (instancetype)initWithFrame:(CGRect)frame {
  NOTREACHED_IN_MIGRATION();
  return nil;
}

#pragma mark Accessors

- (UIScrollView*)scrollView {
  return static_cast<UIScrollView*>(_mockScrollView);
}

- (UIView*)webView {
  return static_cast<UIView*>(_mockWebView);
}

@end
