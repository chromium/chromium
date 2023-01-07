// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WK_UI_HANDLER_DELEGATE_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WK_UI_HANDLER_DELEGATE_H_

#import <WebKit/WebKit.h>

#import "ios/web/web_state/ui/crw_web_view_handler_delegate.h"

@class CRWWKUIHandler;
namespace web {
class WebState;
}

// Delegate for the CRWWKUIHandler.
@protocol CRWWKUIHandlerDelegate <CRWWebViewHandlerDelegate>

// Creates and returns a web view with given `config`, in the `webController`.
- (WKWebView*)UIHandler:(CRWWKUIHandler*)UIHandler
    createWebViewWithConfiguration:(WKWebViewConfiguration*)configuration
                       forWebState:(web::WebState*)webState;

// Returns whether the the action is user initiated.
- (BOOL)UIHandler:(CRWWKUIHandler*)UIHandler
    isUserInitiatedAction:(WKNavigationAction*)action;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WK_UI_HANDLER_DELEGATE_H_
