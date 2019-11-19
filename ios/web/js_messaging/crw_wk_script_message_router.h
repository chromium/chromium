// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_CRW_WK_SCRIPT_MESSAGE_ROUTER_H_
#define IOS_WEB_JS_MESSAGING_CRW_WK_SCRIPT_MESSAGE_ROUTER_H_

#import <WebKit/WebKit.h>

// WKUserContentController wrapper that allows adding multiple message handlers
// for the same message name. CRWWKScriptMessageRouter will route the messages
// from the underlying user content controller to a designated receiver by
// matching the message's name and webView.
@interface CRWWKScriptMessageRouter : NSObject

// Underlying WKUserContentController.
@property(weak, nonatomic, readonly)
    WKUserContentController* userContentController;

// Designated initializer. |userContentController| must not be nil.
- (instancetype)initWithUserContentController:
    (WKUserContentController*)userContentController NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Sets a script message handler. Multiple message handlers can be added for
// the same message name and long as |webView| are different. Setting |handler|
// for the same |name| and |webView| pair is an error. |handler| will be called
// if WKScriptMessage sent by WKUserContentController will match both the |name|
// and the |webView|.
- (void)setScriptMessageHandler:(void (^)(WKScriptMessage*))handler
                           name:(NSString*)messageName
                        webView:(WKWebView*)webView;

// Removes a specific message handler.
- (void)removeScriptMessageHandlerForName:(NSString*)messageName
                                  webView:(WKWebView*)webView;

// Removes all message handlers for the given |webView|.
- (void)removeAllScriptMessageHandlersForWebView:(WKWebView*)webView;

@end

#endif  // IOS_WEB_JS_MESSAGING_CRW_WK_SCRIPT_MESSAGE_ROUTER_H_
