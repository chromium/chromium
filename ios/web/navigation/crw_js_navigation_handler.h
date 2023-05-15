// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_JS_NAVIGATION_HANDLER_H_
#define IOS_WEB_NAVIGATION_CRW_JS_NAVIGATION_HANDLER_H_

#import <WebKit/WebKit.h>

#import "base/values.h"
#import "ios/web/web_state/ui/crw_web_view_handler.h"
#include "url/gurl.h"

namespace web {
class UserInteractionState;
class WebStateImpl;
}  // namespace web

// Handles JS messages related to navigation(e.g. window.history.forward).
@interface CRWJSNavigationHandler : CRWWebViewHandler

// Whether the web page is currently performing window.history.pushState or
// window.history.replaceState.
@property(nonatomic, assign) BOOL changingHistoryState;

// Handles a navigation will change state message for the current webpage.
- (void)handleNavigationWillChangeState;

// Handles a navigation did push state message for the current webpage.
- (void)handleNavigationDidPushStateMessage:(base::Value::Dict*)dict
                                   webState:(web::WebStateImpl*)webStateImpl
                             hasUserGesture:(BOOL)hasUserGesture
                       userInteractionState:
                           (web::UserInteractionState*)userInteractionState
                                 currentURL:(GURL)currentURL;

// Handles a navigation did replace state message for the current webpage.
- (void)handleNavigationDidReplaceStateMessage:(base::Value::Dict*)dict
                                      webState:(web::WebStateImpl*)webStateImpl
                                hasUserGesture:(BOOL)hasUserGesture
                          userInteractionState:
                              (web::UserInteractionState*)userInteractionState
                                    currentURL:(GURL)currentURL;
@end

#endif  // IOS_WEB_NAVIGATION_CRW_JS_NAVIGATION_HANDLER_H_
