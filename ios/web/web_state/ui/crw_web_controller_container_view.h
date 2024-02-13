// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_CONTAINER_VIEW_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import "ios/web/common/crw_content_view.h"

@class CRWWebControllerContainerView;
@class CRWWebViewContentView;
@class CRWWebViewProxyImpl;

@protocol CRWWebControllerContainerViewDelegate<NSObject>

// Returns the proxy object that's backed by the CRWContentView displayed by
// `containerView`.
- (CRWWebViewProxyImpl*)contentViewProxyForContainerView:
        (CRWWebControllerContainerView*)containerView;

// Returns `YES` if the delegate wants to keep the render process alive.
- (BOOL)shouldKeepRenderProcessAliveForContainerView:
    (CRWWebControllerContainerView*)containerView;

// Instructs the delegate to add the `viewToStash` to the view hierarchy to
// keep the render process alive.
- (void)containerView:(CRWWebControllerContainerView*)containerView
    storeWebViewInWindow:(UIView*)viewToStash;

@end

// Container view class that manages the display of content within
// CRWWebController.
@interface CRWWebControllerContainerView : UIView

#pragma mark Content Views
// The web view content view being displayed.
@property(nonatomic, strong, readonly)
    CRWWebViewContentView* webViewContentView;
@property(nonatomic, weak) id<CRWWebControllerContainerViewDelegate>
    delegate;  // weak

// YES if the webView should cover the entire screen and ignore the safe area.
@property(nonatomic, assign) BOOL cover;

// Designated initializer.  `proxy`'s content view will be updated as different
// content is added to the container.
- (instancetype)initWithDelegate:
        (id<CRWWebControllerContainerViewDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

// CRWWebControllerContainerView should be initialized via
// `-initWithDelegate:`.
- (instancetype)initWithCoder:(NSCoder*)decoder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

// Returns YES if the container view is currently displaying content.
- (BOOL)isViewAlive;

// Removes all subviews and resets state to default. The `shutdown` parameter
// indicates if this method was called in a shutdown context.
- (void)resetContentForShutdown:(BOOL)shutdown;

// Replaces the currently displayed content with `webViewContentView`.
- (void)displayWebViewContentView:(CRWWebViewContentView*)webViewContentView;

// Updates the `webViewContentView`'s view hierarchy status based on the the
// container view window status. If the current webView is active but the window
// is nil, store the webView in the view hierarchy keyWindow so WKWebView
// doesn't suspend it's counterpart process.
- (void)updateWebViewContentViewForContainerWindow:(UIWindow*)window;

// Updates `webViewContentView` with the current fullscreen state
- (void)updateWebViewContentViewFullscreenState:
    (CrFullscreenState)fullscreenState;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WEB_CONTROLLER_CONTAINER_VIEW_H_
