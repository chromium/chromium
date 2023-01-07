// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_HANDLER_DELEGATE_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_HANDLER_DELEGATE_H_

#import <Foundation/Foundation.h>

@class CRWWebViewHandler;
class GURL;
@class WKWebView;

namespace web {
class NavigationContextImpl;
class UserInteractionState;
class WebStateImpl;
}

// Delegate for the WebView handlers.
@protocol CRWWebViewHandlerDelegate

// The web state.
- (web::WebStateImpl*)webStateImplForWebViewHandler:(CRWWebViewHandler*)handler;

// The actual URL of the document object.
- (const GURL&)documentURLForWebViewHandler:(CRWWebViewHandler*)handler;

// Asks the delegate for the associated `UserInteractionState`.
- (web::UserInteractionState*)userInteractionStateForWebViewHandler:
    (CRWWebViewHandler*)handler;

// Notifies the delegate that the navigation has finished. Navigation is
// considered complete when the document has finished loading, or when other
// page load mechanics are completed on a non-document-changing URL change.
- (void)webViewHandler:(CRWWebViewHandler*)handler
    didFinishNavigation:(web::NavigationContextImpl*)context;

// Notifies the delegate that the SSL status of the web view changed.
- (void)webViewHandlerUpdateSSLStatusForCurrentNavigationItem:
    (CRWWebViewHandler*)handler;

// The delegate will create a web view if it's not yet created.
- (void)ensureWebViewCreatedForWebViewHandler:(CRWWebViewHandler*)handler;

// Returns the web view.
- (WKWebView*)webViewForWebViewHandler:(CRWWebViewHandler*)handler;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_HANDLER_DELEGATE_H_
