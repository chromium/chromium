// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_WEB_VIEW_NAVIGATION_OBSERVER_H_
#define IOS_WEB_NAVIGATION_CRW_WEB_VIEW_NAVIGATION_OBSERVER_H_

#import <WebKit/WebKit.h>

#import "ios/web/web_state/ui/crw_web_view_handler.h"

@protocol CRWWebViewNavigationObserverDelegate;

// Observes the navigation-related events of a WebView, making sure that the
// different navigaiton events are taken into account.
@interface CRWWebViewNavigationObserver : CRWWebViewHandler

@property(nonatomic, weak) id<CRWWebViewNavigationObserverDelegate> delegate;

// The webView to observe.
@property(nonatomic, weak) WKWebView* webView;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_WEB_VIEW_NAVIGATION_OBSERVER_H_
