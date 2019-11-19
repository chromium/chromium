// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_CONTAINER_VIEW_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/web/common/crw_content_view.h"

@protocol CRWNativeContent;
@protocol CRWNativeContentHolder;
@class CRWWebControllerContainerView;
@class CRWWebViewContentView;
@class CRWWebViewProxyImpl;

@protocol CRWWebControllerContainerViewDelegate<NSObject>

// Returns the proxy object that's backed by the CRWContentView displayed by
// |containerView|.
- (CRWWebViewProxyImpl*)contentViewProxyForContainerView:
        (CRWWebControllerContainerView*)containerView;

// Returns the insets from |containerView|'s bounds in which to lay out native
// content.
- (UIEdgeInsets)nativeContentInsetsForContainerView:
    (CRWWebControllerContainerView*)containerView;

// Returns |YES| if the delegate wants to keep the render process alive.
- (BOOL)shouldKeepRenderProcessAliveForContainerView:
    (CRWWebControllerContainerView*)containerView;

// Instructs the delegate to add the |viewToStash| to the view hierarchy to
// keep the render process alive.
- (void)containerView:(CRWWebControllerContainerView*)containerView
    storeWebViewInWindow:(UIView*)viewToStash;

// Resets the native controller.
- (void)containerViewResetNativeController:
    (CRWWebControllerContainerView*)containerView;

// Returns the native content holder.
- (id<CRWNativeContentHolder>)containerViewNativeContentHolder:
    (CRWWebControllerContainerView*)containerView;

@end

// Container view class that manages the display of content within
// CRWWebController.
@interface CRWWebControllerContainerView : UIView

#pragma mark Content Views
// The web view content view being displayed.
@property(nonatomic, strong, readonly)
    CRWWebViewContentView* webViewContentView;
// The currently displayed transient content view.
@property(nonatomic, strong, readonly) CRWContentView* transientContentView;
@property(nonatomic, weak) id<CRWWebControllerContainerViewDelegate>
    delegate;  // weak

// Designated initializer.  |proxy|'s content view will be updated as different
// content is added to the container.
- (instancetype)initWithDelegate:
        (id<CRWWebControllerContainerViewDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

// CRWWebControllerContainerView should be initialized via
// |-initWithContentViewProxy:|.
- (instancetype)initWithCoder:(NSCoder*)decoder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

// Returns YES if the container view is currently displaying content.
- (BOOL)isViewAlive;

// Removes all subviews and resets state to default.
- (void)resetContent;

// Replaces the currently displayed content with |webViewContentView|.
- (void)displayWebViewContentView:(CRWWebViewContentView*)webViewContentView;

// Notifies the container that the native content changed
- (void)nativeContentDidChange:(id<CRWNativeContent>)previousNativeController;

// Adds |transientContentView| as a subview above previously displayed content.
- (void)displayTransientContent:(CRWContentView*)transientContentView;

// Removes the transient content view, if one is displayed.
- (void)clearTransientContentView;

// Updates the |webViewContentView|'s view hierarchy status based on the the
// container view window status. If the current webView is active but the window
// is nil, store the webView in the view hierarchy keyWindow so WKWebView
// doesn't suspend it's counterpart process.
- (void)updateWebViewContentViewForContainerWindow:(UIWindow*)window;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_CONTAINER_VIEW_H_
