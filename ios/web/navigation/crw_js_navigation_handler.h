// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_JS_NAVIGATION_HANDLER_H_
#define IOS_WEB_NAVIGATION_CRW_JS_NAVIGATION_HANDLER_H_

#import <WebKit/WebKit.h>

#import "ios/web/web_state/ui/crw_web_view_handler.h"
#import "ios/web/web_state/ui/crw_web_view_handler_delegate.h"
#include "url/gurl.h"

@class CRWJSNavigationHandler;

@protocol CRWJSNavigationHandlerDelegate <CRWWebViewHandlerDelegate>

// Returns the current URL of web view.
- (GURL)currentURLForJSNavigationHandler:
    (CRWJSNavigationHandler*)navigationHandler;

// Finds all the scrollviews in the view hierarchy and makes sure they do not
// interfere with scroll to top when tapping the statusbar.
- (void)JSNavigationHandlerOptOutScrollsToTopForSubviews:
    (CRWJSNavigationHandler*)navigationHandler;

@end

// Handles JS messages related to navigation(e.g. window.history.forward).
@interface CRWJSNavigationHandler : CRWWebViewHandler

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDelegate:(id<CRWJSNavigationHandlerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

// Whether the web page is currently performing window.history.pushState or
// window.history.replaceState.
@property(nonatomic, assign) BOOL changingHistoryState;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_JS_NAVIGATION_HANDLER_H_
