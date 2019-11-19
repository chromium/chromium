// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

#import <objc/runtime.h>

#include <memory>

#include "base/auto_reset.h"
#import "base/ios/crb_protocol_observers.h"
#include "base/mac/foundation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWWebViewScrollViewProxy () {
  __weak UIScrollView* _scrollView;
  id _observers;
  std::unique_ptr<UIScrollViewContentInsetAdjustmentBehavior>
      _storedContentInsetAdjustmentBehavior API_AVAILABLE(ios(11.0));
  std::unique_ptr<BOOL> _storedClipsToBounds;
}

// Returns the key paths that need to be observed for UIScrollView.
+ (NSArray*)scrollViewObserverKeyPaths;

// Adds and removes |self| as an observer for |scrollView| with key paths
// returned by |+scrollViewObserverKeyPaths|.
- (void)startObservingScrollView:(UIScrollView*)scrollView;
- (void)stopObservingScrollView:(UIScrollView*)scrollView;

@end

@implementation CRWWebViewScrollViewProxy

- (instancetype)init {
  self = [super init];
  if (self) {
    Protocol* protocol = @protocol(CRWWebViewScrollViewProxyObserver);
    _observers = [CRBProtocolObservers observersWithProtocol:protocol];
  }
  return self;
}

- (void)dealloc {
  [self stopObservingScrollView:_scrollView];
}

- (void)addGestureRecognizer:(UIGestureRecognizer*)gestureRecognizer {
  [_scrollView addGestureRecognizer:gestureRecognizer];
}

- (void)removeGestureRecognizer:(UIGestureRecognizer*)gestureRecognizer {
  [_scrollView removeGestureRecognizer:gestureRecognizer];
}

- (void)addObserver:(id<CRWWebViewScrollViewProxyObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<CRWWebViewScrollViewProxyObserver>)observer {
  [_observers removeObserver:observer];
}

- (void)setScrollView:(UIScrollView*)scrollView {
  if (_scrollView == scrollView)
    return;
  [_scrollView setDelegate:nil];
  [self stopObservingScrollView:_scrollView];
  DCHECK(!scrollView.delegate);
  scrollView.delegate = self;
  [self startObservingScrollView:scrollView];
  _scrollView = scrollView;
  if (_storedClipsToBounds) {
    scrollView.clipsToBounds = *_storedClipsToBounds;
  }

  // Assigns |contentInsetAdjustmentBehavior| which was set before setting the
  // scroll view.
  if (_storedContentInsetAdjustmentBehavior) {
    _scrollView.contentInsetAdjustmentBehavior =
        *_storedContentInsetAdjustmentBehavior;
  }

  [_observers webViewScrollViewProxyDidSetScrollView:self];
}

- (CGRect)frame {
  return _scrollView ? [_scrollView frame] : CGRectZero;
}

- (BOOL)isScrollEnabled {
  return [_scrollView isScrollEnabled];
}

- (void)setScrollEnabled:(BOOL)scrollEnabled {
  [_scrollView setScrollEnabled:scrollEnabled];
}

- (BOOL)bounces {
  return [_scrollView bounces];
}

- (void)setBounces:(BOOL)bounces {
  [_scrollView setBounces:bounces];
}

- (BOOL)clipsToBounds {
  if (!_scrollView && _storedClipsToBounds) {
    return *_storedClipsToBounds;
  }
  return _scrollView.clipsToBounds;
}

- (void)setClipsToBounds:(BOOL)clipsToBounds {
  _storedClipsToBounds = std::make_unique<BOOL>(clipsToBounds);
  _scrollView.clipsToBounds = clipsToBounds;
}

- (BOOL)isDecelerating {
  return [_scrollView isDecelerating];
}

- (BOOL)isDragging {
  return [_scrollView isDragging];
}

- (BOOL)isTracking {
  return [_scrollView isTracking];
}

- (BOOL)isZooming {
  return [_scrollView isZooming];
}

- (CGFloat)zoomScale {
  return [_scrollView zoomScale];
}

- (void)setContentOffset:(CGPoint)contentOffset {
  [_scrollView setContentOffset:contentOffset];
}

- (CGPoint)contentOffset {
  return _scrollView ? [_scrollView contentOffset] : CGPointZero;
}

- (void)setContentInset:(UIEdgeInsets)contentInset {
  [_scrollView setContentInset:contentInset];
}

- (UIEdgeInsets)contentInset {
  return _scrollView ? [_scrollView contentInset] : UIEdgeInsetsZero;
}

- (void)setScrollIndicatorInsets:(UIEdgeInsets)scrollIndicatorInsets {
  [_scrollView setScrollIndicatorInsets:scrollIndicatorInsets];
}

- (UIEdgeInsets)scrollIndicatorInsets {
  return _scrollView ? [_scrollView scrollIndicatorInsets] : UIEdgeInsetsZero;
}

- (void)setContentSize:(CGSize)contentSize {
  [_scrollView setContentSize:contentSize];
}

- (CGSize)contentSize {
  return _scrollView ? [_scrollView contentSize] : CGSizeZero;
}

- (void)setContentOffset:(CGPoint)contentOffset animated:(BOOL)animated {
  [_scrollView setContentOffset:contentOffset animated:animated];
}

- (BOOL)scrollsToTop {
  return [_scrollView scrollsToTop];
}

- (void)setScrollsToTop:(BOOL)scrollsToTop {
  [_scrollView setScrollsToTop:scrollsToTop];
}

- (UIScrollViewContentInsetAdjustmentBehavior)contentInsetAdjustmentBehavior
    API_AVAILABLE(ios(11.0)) {
  if (_scrollView) {
    return [_scrollView contentInsetAdjustmentBehavior];
  } else if (_storedContentInsetAdjustmentBehavior) {
    return *_storedContentInsetAdjustmentBehavior;
  } else {
    return UIScrollViewContentInsetAdjustmentAutomatic;
  }
}

- (UIEdgeInsets)adjustedContentInset API_AVAILABLE(ios(11.0)) {
  return [_scrollView adjustedContentInset];
}

- (void)setContentInsetAdjustmentBehavior:
    (UIScrollViewContentInsetAdjustmentBehavior)contentInsetAdjustmentBehavior
    API_AVAILABLE(ios(11.0)) {
  [_scrollView
      setContentInsetAdjustmentBehavior:contentInsetAdjustmentBehavior];
  _storedContentInsetAdjustmentBehavior =
      std::make_unique<UIScrollViewContentInsetAdjustmentBehavior>(
          contentInsetAdjustmentBehavior);
}

- (UIPanGestureRecognizer*)panGestureRecognizer {
  return [_scrollView panGestureRecognizer];
}

- (NSArray*)gestureRecognizers {
  return [_scrollView gestureRecognizers];
}

- (NSArray<__kindof UIView*>*)subviews {
  return _scrollView ? [_scrollView subviews] : @[];
}

#pragma mark -
#pragma mark UIScrollViewDelegate callbacks

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  DCHECK_EQ(_scrollView, scrollView);
  [_observers webViewScrollViewDidScroll:self];
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  DCHECK_EQ(_scrollView, scrollView);
  [_observers webViewScrollViewWillBeginDragging:self];
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  DCHECK_EQ(_scrollView, scrollView);
  [_observers webViewScrollViewWillEndDragging:self
                                  withVelocity:velocity
                           targetContentOffset:targetContentOffset];
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  DCHECK_EQ(_scrollView, scrollView);
  [_observers webViewScrollViewDidEndDragging:self willDecelerate:decelerate];
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  DCHECK_EQ(_scrollView, scrollView);
  [_observers webViewScrollViewDidEndDecelerating:self];
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView*)scrollView {
  DCHECK_EQ(_scrollView, scrollView);
  [_observers webViewScrollViewDidEndScrollingAnimation:self];
}

- (BOOL)scrollViewShouldScrollToTop:(UIScrollView*)scrollView {
  DCHECK_EQ(_scrollView, scrollView);
  __block BOOL shouldScrollToTop = YES;
  [_observers executeOnObservers:^(id observer) {
    if ([observer respondsToSelector:@selector
                  (webViewScrollViewShouldScrollToTop:)]) {
      shouldScrollToTop = shouldScrollToTop &&
                          [observer webViewScrollViewShouldScrollToTop:self];
    }
  }];
  return shouldScrollToTop;
}

- (void)scrollViewDidZoom:(UIScrollView*)scrollView {
  DCHECK_EQ(_scrollView, scrollView);
  [_observers webViewScrollViewDidZoom:self];
}

- (void)scrollViewWillBeginZooming:(UIScrollView*)scrollView
                          withView:(UIView*)view {
  DCHECK_EQ(_scrollView, scrollView);
  [_observers webViewScrollViewWillBeginZooming:self];
}

- (void)scrollViewDidEndZooming:(UIScrollView*)scrollView
                       withView:(UIView*)view
                        atScale:(CGFloat)scale {
  DCHECK_EQ(_scrollView, scrollView);
  [_observers webViewScrollViewDidEndZooming:self atScale:scale];
}

#pragma mark -

+ (NSArray*)scrollViewObserverKeyPaths {
  return @[ @"frame", @"contentSize", @"contentInset" ];
}

- (void)startObservingScrollView:(UIScrollView*)scrollView {
  for (NSString* keyPath in [[self class] scrollViewObserverKeyPaths])
    [scrollView addObserver:self forKeyPath:keyPath options:0 context:nil];
}

- (void)stopObservingScrollView:(UIScrollView*)scrollView {
  for (NSString* keyPath in [[self class] scrollViewObserverKeyPaths])
    [scrollView removeObserver:self forKeyPath:keyPath];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  DCHECK_EQ(object, _scrollView);
  if ([keyPath isEqualToString:@"frame"])
    [_observers webViewScrollViewFrameDidChange:self];
  if ([keyPath isEqualToString:@"contentSize"])
    [_observers webViewScrollViewDidResetContentSize:self];
  if ([keyPath isEqualToString:@"contentInset"])
    [_observers webViewScrollViewDidResetContentInset:self];
}

@end
