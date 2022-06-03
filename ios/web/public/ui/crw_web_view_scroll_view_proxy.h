// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_UI_CRW_WEB_VIEW_SCROLL_VIEW_PROXY_H_
#define IOS_WEB_PUBLIC_UI_CRW_WEB_VIEW_SCROLL_VIEW_PROXY_H_

#import <UIKit/UIKit.h>

@protocol CRWWebViewScrollViewProxyObserver;

// Provides an interface for web state observers to access the WebState's
// UIScrollView in a limited and controlled manner.
// This class is designed to limit lifetime of the UIScrollView such that it is
// not retained beyond WebState. It is also a way to tunnel UIScrollViewDelegate
// callbacks.
// NOTE: The API exposed by the proxy class isn't intended to be restrictive.
// The features needing to access other UIScrollView properties and methods
// needed to drive the UIScrollView are free to extend the proxy class as
// needed.
// The class forwards some of the methods onto the UIScrollView. For more
// information look at the UIScrollView documentation.
// TODO(crbug.com/546152): rename class to CRWContentViewScrollViewProxy.
@interface CRWWebViewScrollViewProxy : NSObject

@property(nonatomic, readonly, copy) NSArray<__kindof UIView*>* subviews;

// Used by the CRWWebViewProxy to set the UIScrollView to be managed.
- (void)setScrollView:(UIScrollView*)scrollView;

// Adds |observer| to subscribe to change notifications.
- (void)addObserver:(id<CRWWebViewScrollViewProxyObserver>)observer;

// Removes |observer| as a subscriber for change notifications.
- (void)removeObserver:(id<CRWWebViewScrollViewProxyObserver>)observer;

// Returns a scroll view proxy which can be accessed as UIScrollView.
//
// Note: This method is introduced because CRWWebViewScrollViewProxy cannot
// simply be a subclass of UIScrollView. Its implementation relies on
// -forwardInvocation: which only works for methods not implemented in the class
// (including those implemented in its superclass). So -forwardInvocation:
// cannot forward methods in UIScrollView if CRWWebViewScrollViewProxy is a
// subclass of UIScrollView.
- (UIScrollView*)asUIScrollView;

@end

// Methods/properties implemented by -forwardInvocation:. Declared in a category
// to avoid compilation error saying they are not implemented.
@interface CRWWebViewScrollViewProxy (ForwardedMethods)

@property(nonatomic, assign) CGPoint contentOffset;
@property(nonatomic, assign) UIEdgeInsets contentInset;
@property(nonatomic, readonly, getter=isDecelerating) BOOL decelerating;
@property(nonatomic, readonly, getter=isDragging) BOOL dragging;
@property(nonatomic, readonly, getter=isTracking) BOOL tracking;
@property(nonatomic, readonly) BOOL isZooming;
@property(nonatomic, readonly) CGFloat zoomScale;
@property(nonatomic, assign) UIEdgeInsets scrollIndicatorInsets;
@property(nonatomic, assign) CGSize contentSize;
@property(nonatomic, readonly) CGRect frame;
@property(nonatomic, getter=isScrollEnabled) BOOL scrollEnabled;
@property(nonatomic, assign) BOOL bounces;
@property(nonatomic, assign) BOOL scrollsToTop;
@property(nonatomic, assign) BOOL clipsToBounds;
@property(nonatomic, assign)
    UIScrollViewContentInsetAdjustmentBehavior contentInsetAdjustmentBehavior
        API_AVAILABLE(ios(11.0));
@property(nonatomic, readonly)
    UIEdgeInsets adjustedContentInset API_AVAILABLE(ios(11.0));
@property(weak, nonatomic, readonly)
    UIPanGestureRecognizer* panGestureRecognizer;
// Returns the scrollview's gesture recognizers.
@property(weak, nonatomic, readonly) NSArray* gestureRecognizers;

- (void)addGestureRecognizer:(UIGestureRecognizer*)gestureRecognizer;
- (void)removeGestureRecognizer:(UIGestureRecognizer*)gestureRecognizer;
- (void)setContentOffset:(CGPoint)contentOffset animated:(BOOL)animated;

@end

// A protocol to be implemented by objects to listen for changes to the
// UIScrollView.
// This is an exact mirror of the UIScrollViewDelegate callbacks. For more
// information look at the UIScrollViewDelegate documentation.
@protocol CRWWebViewScrollViewObserver <NSObject>
@optional
- (void)webViewScrollViewFrameDidChange:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy;
- (void)webViewScrollViewDidScroll:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy;
- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy;
- (void)webViewScrollViewWillEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                            withVelocity:(CGPoint)velocity
                     targetContentOffset:(inout CGPoint*)targetContentOffset;
- (void)webViewScrollViewDidEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                         willDecelerate:(BOOL)decelerate;
- (void)webViewScrollViewDidEndScrollingAnimation:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy;
- (void)webViewScrollViewDidEndDecelerating:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy;
- (BOOL)webViewScrollViewShouldScrollToTop:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy;
- (void)webViewScrollViewDidZoom:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy;
- (void)webViewScrollViewDidResetContentSize:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy;
- (void)webViewScrollViewDidResetContentInset:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy;

// The equivalent in UIScrollViewDelegate also takes a parameter (UIView*)view,
// but CRWWebViewScrollViewObserver doesn't expose it for flexibility of future
// implementation.
- (void)webViewScrollViewWillBeginZooming:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy;
- (void)webViewScrollViewDidEndZooming:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                               atScale:(CGFloat)scale;

@end

// A protocol to be implemented by objects to listen for changes to the
// CRWWebViewScrollViewProxyObserver.
// It inherit from CRWWebViewScrollViewScrollViewObserver which only implements
// methods for listening to scrollview changes.
@protocol CRWWebViewScrollViewProxyObserver <CRWWebViewScrollViewObserver>
@optional
// Called when the underlying scrollview of the proxy is set.
- (void)webViewScrollViewProxyDidSetScrollView:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy;
@end

#endif  // IOS_WEB_PUBLIC_UI_CRW_WEB_VIEW_SCROLL_VIEW_PROXY_H_
