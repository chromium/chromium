// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_scroll_view.h"

#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web_view/internal/cwv_scroll_view_internal.h"
#import "ios/web_view/public/cwv_scroll_view_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CWVScrollView ()<CRWWebViewScrollViewProxyObserver>

// For KVO compliance, redefines the property as readwrite and calls its setter
// when the value changes, instead of defining a getter which directly calls
// _proxy.contentSize.
@property(nonatomic, readwrite) CGSize contentSize;

@end

@implementation CWVScrollView

@synthesize contentSize = _contentSize;
@synthesize contentOffset = _contentOffset;
@synthesize delegate = _delegate;
@synthesize proxy = _proxy;

- (void)setProxy:(nullable CRWWebViewScrollViewProxy*)proxy {
  [_proxy removeObserver:self];
  _proxy = proxy;
  self.contentSize = _proxy.contentSize;
  [self updateContentOffset];
  [_proxy addObserver:self];
}

- (void)setContentOffset:(CGPoint)contentOffset {
  _proxy.contentOffset = contentOffset;
  [self updateContentOffset];
}

- (UIEdgeInsets)scrollIndicatorInsets {
  return _proxy.scrollIndicatorInsets;
}

- (void)setScrollIndicatorInsets:(UIEdgeInsets)scrollIndicatorInsets {
  _proxy.scrollIndicatorInsets = scrollIndicatorInsets;
}

- (CGRect)bounds {
  return {_proxy.contentOffset, _proxy.frame.size};
}

- (BOOL)isDecelerating {
  return _proxy.decelerating;
}

- (BOOL)isDragging {
  return _proxy.dragging;
}

- (BOOL)isTracking {
  return _proxy.tracking;
}

- (BOOL)scrollsToTop {
  return _proxy.scrollsToTop;
}

- (void)setScrollsToTop:(BOOL)scrollsToTop {
  _proxy.scrollsToTop = scrollsToTop;
}

- (BOOL)bounces {
  return _proxy.bounces;
}

- (void)setBounces:(BOOL)bounces {
  _proxy.bounces = bounces;
}

- (UIScrollViewContentInsetAdjustmentBehavior)contentInsetAdjustmentBehavior
    API_AVAILABLE(ios(11.0)) {
  return _proxy.contentInsetAdjustmentBehavior;
}

- (void)setContentInsetAdjustmentBehavior:
    (UIScrollViewContentInsetAdjustmentBehavior)contentInsetAdjustmentBehavior
    API_AVAILABLE(ios(11.0)) {
  _proxy.contentInsetAdjustmentBehavior = contentInsetAdjustmentBehavior;
}

- (UIPanGestureRecognizer*)panGestureRecognizer {
  return _proxy.panGestureRecognizer;
}

- (NSArray<__kindof UIView*>*)subviews {
  return _proxy.subviews;
}

- (UIEdgeInsets)contentInset {
  return _proxy.contentInset;
}

- (void)setContentInset:(UIEdgeInsets)contentInset {
  _proxy.contentInset = contentInset;
}

- (void)setContentOffset:(CGPoint)contentOffset animated:(BOOL)animated {
  [_proxy setContentOffset:contentOffset animated:animated];
  [self updateContentOffset];
}

- (void)addGestureRecognizer:(UIGestureRecognizer*)gestureRecognizer {
  [_proxy addGestureRecognizer:gestureRecognizer];
}

- (void)removeGestureRecognizer:(UIGestureRecognizer*)gestureRecognizer {
  [_proxy removeGestureRecognizer:gestureRecognizer];
}

#pragma mark - NSObject

- (void)dealloc {
  // Removes |self| from |_proxy|'s observers. Otherwise |_proxy| will keep a
  // dangling pointer to |self| and cause SEGV later.
  [_proxy removeObserver:self];
}

#pragma mark - CRWWebViewScrollViewObserver

- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  SEL selector = @selector(scrollViewWillBeginDragging:);
  if ([_delegate respondsToSelector:selector]) {
    [_delegate scrollViewWillBeginDragging:self];
  }
}
- (void)webViewScrollViewWillEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                            withVelocity:(CGPoint)velocity
                     targetContentOffset:(inout CGPoint*)targetContentOffset {
  SEL selector =
      @selector(scrollViewWillEndDragging:withVelocity:targetContentOffset:);
  if ([_delegate respondsToSelector:selector]) {
    [_delegate scrollViewWillEndDragging:self
                            withVelocity:velocity
                     targetContentOffset:targetContentOffset];
  }
}

- (void)webViewScrollViewDidScroll:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  [self updateContentOffset];
  SEL selector = @selector(scrollViewDidScroll:);
  if ([_delegate respondsToSelector:selector]) {
    [_delegate scrollViewDidScroll:self];
  }
}

- (void)webViewScrollViewDidEndDecelerating:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  SEL selector = @selector(scrollViewDidEndDecelerating:);
  if ([_delegate respondsToSelector:selector]) {
    [_delegate scrollViewDidEndDecelerating:self];
  }
}

- (void)webViewScrollViewWillBeginZooming:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  SEL selector = @selector(scrollViewWillBeginZooming:);
  if ([_delegate respondsToSelector:selector]) {
    [_delegate scrollViewWillBeginZooming:self];
  }
}

- (BOOL)webViewScrollViewShouldScrollToTop:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  if ([_delegate respondsToSelector:@selector(scrollViewShouldScrollToTop:)]) {
    return [_delegate scrollViewShouldScrollToTop:self];
  }
  return YES;
}

- (void)webViewScrollViewDidResetContentSize:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  self.contentSize = _proxy.contentSize;
}

- (void)updateContentOffset {
  [self willChangeValueForKey:@"contentOffset"];
  _contentOffset = _proxy.contentOffset;
  [self didChangeValueForKey:@"contentOffset"];
}

+ (BOOL)automaticallyNotifiesObserversForKey:(NSString*)key {
  if ([key isEqualToString:@"contentOffset"]) {
    return NO;
  }
  return [super automaticallyNotifiesObserversForKey:key];
}

@end
