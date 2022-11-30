// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/gestures/pan_handler_scroll_view.h"

#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PanHandlerScrollView ()

@property(nonatomic, strong) UIScrollView* scrollView;
@property(nonatomic, strong) CRWWebViewScrollViewProxy* scrollViewProxy;

@end

@implementation PanHandlerScrollView

- (instancetype)initWithScrollView:(UIScrollView*)scrollView {
  if (self = [super init]) {
    _scrollView = scrollView;
  }
  return self;
}

- (instancetype)initWithWebViewScrollViewProxy:
    (CRWWebViewScrollViewProxy*)scrollViewProxy {
  if (self = [super init]) {
    _scrollViewProxy = scrollViewProxy;
  }
  return self;
}

- (CGPoint)contentOffset {
  return (self.scrollView) ? self.scrollView.contentOffset
                           : self.scrollViewProxy.contentOffset;
}

- (void)setContentOffset:(CGPoint)contentOffset {
  self.scrollView.contentOffset = contentOffset;
  self.scrollViewProxy.contentOffset = contentOffset;
}

- (UIEdgeInsets)contentInset {
  return (self.scrollView) ? self.scrollView.contentInset
                           : self.scrollViewProxy.contentInset;
}

- (void)setContentInset:(UIEdgeInsets)contentInset {
  self.scrollView.contentInset = contentInset;
  self.scrollViewProxy.contentInset = contentInset;
}

- (UIPanGestureRecognizer*)panGestureRecognizer {
  return (self.scrollView) ? self.scrollView.panGestureRecognizer
                           : self.scrollViewProxy.panGestureRecognizer;
}

- (BOOL)isDecelerating {
  return (self.scrollView) ? self.scrollView.isDecelerating
                           : self.scrollViewProxy.isDecelerating;
}

- (BOOL)isDragging {
  return (self.scrollView) ? self.scrollView.isDragging
                           : self.scrollViewProxy.isDragging;
}

@end
