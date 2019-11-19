// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WK_UI_HANDLER_DELEGATE_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WK_UI_HANDLER_DELEGATE_H_

#import <WebKit/WebKit.h>

@class CRWWebController;
@class CRWWKUIHandler;
class GURL;
namespace web {
class WebStateImpl;
class WebState;
}

// Delegate for the CRWWKUIHandler.
@protocol CRWWKUIHandlerDelegate

// Returns the URL of the document object (i.e. last committed URL).
- (const GURL&)documentURLForUIHandler:(CRWWKUIHandler*)UIHandler;

// Creates and returns a web view with given |config|, in the |webController|.
- (WKWebView*)UIHandler:(CRWWKUIHandler*)UIHandler
    createWebViewWithConfiguration:(WKWebViewConfiguration*)configuration
                       forWebState:(web::WebState*)webState;

// Returns whether the the action is user initiated.
- (BOOL)UIHandler:(CRWWKUIHandler*)UIHandler
    isUserInitiatedAction:(WKNavigationAction*)action;

// Returns the WebStateImpl.
- (web::WebStateImpl*)webStateImplForUIHandler:(CRWWKUIHandler*)UIHandler;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WK_UI_HANDLER_DELEGATE_H_
