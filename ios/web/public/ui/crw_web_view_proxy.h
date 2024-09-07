// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_UI_CRW_WEB_VIEW_PROXY_H_
#define IOS_WEB_PUBLIC_UI_CRW_WEB_VIEW_PROXY_H_

#import <UIKit/UIKit.h>

@class CRWWebViewScrollViewProxy;

// Provides an interface for embedders to access the WebState's web view in a
// limited and controlled manner.
// TODO(crbug.com/41211285): rename protocol to CRWContentViewProxy.
@protocol CRWWebViewProxy <NSObject>

// The web view's bounding rectangle (relative to its parent).
@property(readonly, assign) CGRect bounds;

// The web view's frame rectangle.
@property(readonly, assign) CGRect frame;

// Adds an offset to the scrollable content's frame.
@property(nonatomic, assign) CGPoint contentOffset;

// Adds an inset to the content view. Implementations of this protocol can
// implement this method using UIScrollView.contentInset (where applicable) or
// via resizing a subview's frame. Changing this property may impact performance
// if implementation resizes its subview. Can be used as a workaround for
// WKWebView bug, where UIScrollView.content inset does not work
// (rdar://23584409). TODO(crbug.com/41228596) remove this property once radar
// is fixed.
@property(nonatomic, assign) UIEdgeInsets contentInset;

// Gives the embedder access to the web view's UIScrollView in a limited and
// controlled manner.
@property(nonatomic, readonly) CRWWebViewScrollViewProxy* scrollViewProxy;

// A Boolean value indicating whether horizontal swipe gestures will trigger
// back-forward list navigations.
@property(nonatomic) BOOL allowsBackForwardNavigationGestures;

// Returns the webview's gesture recognizers.
@property(nonatomic, readonly) NSArray* gestureRecognizers;

// A Boolean value indicating whether or not the web page is in fullscreen mode.
@property(nonatomic, readonly) BOOL isWebPageInFullscreenMode;

// Whether or not the content view should use the content inset when setting
// `contentInset`. Implementations may or may not respect the setting of this
// property.
@property(nonatomic, assign) BOOL shouldUseViewContentInset;

// Register the given insets for the given caller.
- (void)registerInsets:(UIEdgeInsets)insets forCaller:(id)caller;

// Unregister the registered insets for the given caller.
- (void)unregisterInsetsForCaller:(id)caller;

// Wrapper around the addSubview method of the webview.
- (void)addSubview:(UIView*)view;

// YES if the keyboard is currently visible for use in the web view.
@property(nonatomic, readonly, getter=isKeyboardVisible) BOOL keyboardVisible;

// Wrapper around the becomeFirstResponder method of the webview.
- (BOOL)becomeFirstResponder;

@end

#endif  // IOS_WEB_PUBLIC_UI_CRW_WEB_VIEW_PROXY_H_
