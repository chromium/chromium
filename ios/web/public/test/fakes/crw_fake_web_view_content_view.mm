// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/crw_fake_web_view_content_view.h"

#import "base/check.h"
#import "base/notreached.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  NOTREACHED();
  return nil;
}

- (instancetype)initWithFrame:(CGRect)frame {
  NOTREACHED();
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
