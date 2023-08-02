// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_view_scroll_view_delegate_proxy.h"

#import <ostream>

#import "base/check_op.h"
#import "base/ios/crb_protocol_observers.h"
#import "ios/web/web_state/ui/crw_web_view_scroll_view_proxy+internal.h"

@interface CRWWebViewScrollViewDelegateProxy ()

@property(nonatomic, weak) CRWWebViewScrollViewProxy* scrollViewProxy;

// Return YES if the user is currently performing a zoom gestures.
@property(nonatomic, assign) BOOL userIsZooming;

@end

// Calls to methods supported by CRWWebViewScrollViewProxyObserver are forwarded
// to both of the delegate and the observers of self.scrollViewProxy.
// Calls to other methods are forwarded only to self.delegateOfProxy
// using -methodSignatoreForSelector: and -forwardInvocation:.
@implementation CRWWebViewScrollViewDelegateProxy

- (instancetype)initWithScrollViewProxy:
    (CRWWebViewScrollViewProxy*)scrollViewProxy {
  self = [super init];
  if (self) {
    _scrollViewProxy = scrollViewProxy;
  }
  return self;
}

#pragma mark - NSObject

- (BOOL)respondsToSelector:(SEL)aSelector {
  // This class forwards unimplemented methods to the delegate of the scroll
  // view proxy. So it also responds to methods defined in the delegate of the
  // scroll view proxy.
  return [self.delegateOfProxy respondsToSelector:aSelector] ||
         [super respondsToSelector:aSelector];
}

#pragma mark Forwards unimplemented methods

- (NSMethodSignature*)methodSignatureForSelector:(SEL)sel {
  // Called when the method is not implemented in this class. Forwards the
  // method to the delegate of the scroll view proxy.

  // This cast is necessary because -methodSignatureForSelector: is a method of
  // NSObject. It is pretty safe to assume that the delegate is an instance of
  // NSObject.
  NSObject* delegateAsObject = static_cast<NSObject*>(self.delegateOfProxy);

  return [delegateAsObject methodSignatureForSelector:sel];
}

- (void)forwardInvocation:(NSInvocation*)invocation {
  // Called when the method is not implemented in this class. Forwards the
  // method to the delegate of the scroll view proxy.

  // Replaces the `sender` argument of the delegate method call with
  // [self.scrollViewProxy asUIScrollView]. `sender` should be the first
  // argument of every delegate method according to Apple's style guide:
  // https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/CodingGuidelines/Articles/NamingMethods.html#//apple_ref/doc/uid/20001282-BCIGIJJF
  // and it is true for all methods of UIScrollViewDelegate as of today. But
  // here performs a few safety checks to make sure that the first argument is
  // `sender`:
  //   - The method has at least one argument
  //   - The first argument is typed UIScrollView
  //   - The first argument is equal to the underlying scroll view
  //
  // Note that the first (normal) argument is at index 2. Index 0 and 1 are for
  // self and _cmd respectively.
  NSMethodSignature* signature = invocation.methodSignature;
  if (signature.numberOfArguments >= 3 &&
      strcmp([signature getArgumentTypeAtIndex:2], @encode(UIScrollView*)) ==
          0) {
    __unsafe_unretained UIScrollView* sender;
    [invocation getArgument:&sender atIndex:2];
    if (sender == self.scrollViewProxy.underlyingScrollView) {
      sender = [self.scrollViewProxy asUIScrollView];
      [invocation setArgument:&sender atIndex:2];
    }
  }

  [invocation invokeWithTarget:self.delegateOfProxy];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  if ([self.delegateOfProxy
          respondsToSelector:@selector(scrollViewDidScroll:)]) {
    [self.delegateOfProxy
        scrollViewDidScroll:[self.scrollViewProxy asUIScrollView]];
  }
  [self.scrollViewProxy.observers
      webViewScrollViewDidScroll:self.scrollViewProxy];
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  if ([self.delegateOfProxy
          respondsToSelector:@selector(scrollViewWillBeginDragging:)]) {
    [self.delegateOfProxy
        scrollViewWillBeginDragging:[self.scrollViewProxy asUIScrollView]];
  }
  [self.scrollViewProxy.observers
      webViewScrollViewWillBeginDragging:self.scrollViewProxy];
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  if ([self.delegateOfProxy respondsToSelector:@selector
                            (scrollViewWillEndDragging:
                                          withVelocity:targetContentOffset:)]) {
    [self.delegateOfProxy
        scrollViewWillEndDragging:[self.scrollViewProxy asUIScrollView]
                     withVelocity:velocity
              targetContentOffset:targetContentOffset];
  }
  [self.scrollViewProxy.observers
      webViewScrollViewWillEndDragging:self.scrollViewProxy
                          withVelocity:velocity
                   targetContentOffset:targetContentOffset];
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  if ([self.delegateOfProxy respondsToSelector:@selector
                            (scrollViewDidEndDragging:willDecelerate:)]) {
    [self.delegateOfProxy
        scrollViewDidEndDragging:[self.scrollViewProxy asUIScrollView]
                  willDecelerate:decelerate];
  }
  [self.scrollViewProxy.observers
      webViewScrollViewDidEndDragging:self.scrollViewProxy
                       willDecelerate:decelerate];
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  if ([self.delegateOfProxy
          respondsToSelector:@selector(scrollViewDidEndDecelerating:)]) {
    [self.delegateOfProxy
        scrollViewDidEndDecelerating:[self.scrollViewProxy asUIScrollView]];
  }
  [self.scrollViewProxy.observers
      webViewScrollViewDidEndDecelerating:self.scrollViewProxy];
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView*)scrollView {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  if ([self.delegateOfProxy
          respondsToSelector:@selector(scrollViewDidEndScrollingAnimation:)]) {
    [self.delegateOfProxy
        scrollViewDidEndScrollingAnimation:[self.scrollViewProxy
                                                   asUIScrollView]];
  }
  [self.scrollViewProxy.observers
      webViewScrollViewDidEndScrollingAnimation:self.scrollViewProxy];
}

- (BOOL)scrollViewShouldScrollToTop:(UIScrollView*)scrollView {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  __block BOOL shouldScrollToTop = YES;

  if ([self.delegateOfProxy
          respondsToSelector:@selector(scrollViewShouldScrollToTop:)]) {
    shouldScrollToTop =
        shouldScrollToTop &&
        [self.delegateOfProxy
            scrollViewShouldScrollToTop:[self.scrollViewProxy asUIScrollView]];
  }

  [self.scrollViewProxy.observers executeOnObservers:^(id observer) {
    if ([observer respondsToSelector:@selector
                  (webViewScrollViewShouldScrollToTop:)]) {
      shouldScrollToTop =
          shouldScrollToTop &&
          [observer webViewScrollViewShouldScrollToTop:self.scrollViewProxy];
    }
  }];

  return shouldScrollToTop;
}

- (void)scrollViewDidZoom:(UIScrollView*)scrollView {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  if ([self.delegateOfProxy respondsToSelector:@selector(scrollViewDidZoom:)]) {
    [self.delegateOfProxy
        scrollViewDidZoom:[self.scrollViewProxy asUIScrollView]];
  }
  if (self.userIsZooming) {
    [self.scrollViewProxy.observers
        webViewScrollViewDidZoom:self.scrollViewProxy];
  } else {
    if (@available(iOS 16.0, *)) {
      // In iOS < 16 versions, changing the value of `zoomScale` calls
      // `scrollViewDidZoom`.
      scrollView.zoomScale = scrollView.minimumZoomScale;
    }
  }
}

- (void)scrollViewWillBeginZooming:(UIScrollView*)scrollView
                          withView:(UIView*)view {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  self.userIsZooming = YES;
  if ([self.delegateOfProxy
          respondsToSelector:@selector(scrollViewWillBeginZooming:withView:)]) {
    [self.delegateOfProxy
        scrollViewWillBeginZooming:[self.scrollViewProxy asUIScrollView]
                          withView:view];
  }
  [self.scrollViewProxy.observers
      webViewScrollViewWillBeginZooming:self.scrollViewProxy];
}

- (void)scrollViewDidEndZooming:(UIScrollView*)scrollView
                       withView:(UIView*)view
                        atScale:(CGFloat)scale {
  DCHECK_EQ(self.scrollViewProxy.underlyingScrollView, scrollView);
  self.userIsZooming = NO;
  if ([self.delegateOfProxy respondsToSelector:@selector
                            (scrollViewDidEndZooming:withView:atScale:)]) {
    [self.delegateOfProxy
        scrollViewDidEndZooming:[self.scrollViewProxy asUIScrollView]
                       withView:view
                        atScale:scale];
  }
  [self.scrollViewProxy.observers
      webViewScrollViewDidEndZooming:self.scrollViewProxy
                             atScale:scale];
}

#pragma mark - Helpers

// The delegate of the scroll view proxy.
- (id<UIScrollViewDelegate>)delegateOfProxy {
  return [self.scrollViewProxy asUIScrollView].delegate;
}

@end
