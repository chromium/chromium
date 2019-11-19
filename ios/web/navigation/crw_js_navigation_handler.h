// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_JS_NAVIGATION_HANDLER_H_
#define IOS_WEB_NAVIGATION_CRW_JS_NAVIGATION_HANDLER_H_

#import <WebKit/WebKit.h>

#include "url/gurl.h"

namespace web {
class WebStateImpl;
class UserInteractionState;
class NavigationContextImpl;
}
@class CRWJSNavigationHandler;

@protocol CRWJSNavigationHandlerDelegate

// Returns the WebStateImpl associated with this handler.
- (web::WebStateImpl*)webStateImplForJSNavigationHandler:
    (CRWJSNavigationHandler*)navigationHandler;

// Returns the current URL of web view.
- (GURL)currentURLForJSNavigationHandler:
    (CRWJSNavigationHandler*)navigationHandler;

// Returns associated UserInteractionState.
- (web::UserInteractionState*)userInteractionStateForJSNavigationHandler:
    (CRWJSNavigationHandler*)navigationHandler;

// Returns associated WKWebView.
- (WKWebView*)webViewForJSNavigationHandler:
    (CRWJSNavigationHandler*)navigationHandler;

// Instructs the delegate to update SSL status.
- (void)JSNavigationHandlerUpdateSSLStatusForCurrentNavigationItem:
    (CRWJSNavigationHandler*)navigationHandler;

// Finds all the scrollviews in the view hierarchy and makes sure they do not
// interfere with scroll to top when tapping the statusbar.
- (void)JSNavigationHandlerOptOutScrollsToTopForSubviews:
    (CRWJSNavigationHandler*)navigationHandler;

// Notifies the delegate that navigation has finished.
- (void)JSNavigationHandler:(CRWJSNavigationHandler*)navigationHandler
        didFinishNavigation:(web::NavigationContextImpl*)context;

// Instructs the delegate to reload a rendered initiated reload.
- (void)JSNavigationHandlerReloadWithRendererInitiatedNavigation:
    (CRWJSNavigationHandler*)navigationHandler;

@end

// Handles JS messages related to navigation(e.g. window.history.forward).
@interface CRWJSNavigationHandler : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDelegate:(id<CRWJSNavigationHandlerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

// Set to YES when a hashchange event is manually dispatched for same-document
// history navigations.
@property(nonatomic, assign) BOOL dispatchingSameDocumentHashChangeEvent;

// Whether the web page is currently performing window.history.pushState or
// window.history.replaceState.
@property(nonatomic, assign) BOOL changingHistoryState;

// Instructs this handler to stop handling js navigation messages.
- (void)close;

// Generates the JavaScript string used to update the UIWebView's URL so that it
// matches the URL displayed in the omnibox and sets window.history.state to
// stateObject. Needed for history.pushState() and history.replaceState().
- (NSString*)javaScriptToReplaceWebViewURL:(const GURL&)URL
                           stateObjectJSON:(NSString*)stateObject;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_JS_NAVIGATION_HANDLER_H_
