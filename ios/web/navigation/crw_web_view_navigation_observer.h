// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_WEB_VIEW_NAVIGATION_OBSERVER_H_
#define IOS_WEB_NAVIGATION_CRW_WEB_VIEW_NAVIGATION_OBSERVER_H_

#import <WebKit/WebKit.h>

@protocol CRWWebViewNavigationObserverDelegate;

// Observes the navigation-related events of a WebView, making sure that the
// different navigaiton events are taken into account.
@interface CRWWebViewNavigationObserver : NSObject

@property(nonatomic, weak) id<CRWWebViewNavigationObserverDelegate> delegate;

// The webView to observe.
@property(nonatomic, weak) WKWebView* webView;

// Instructs this handler to close.
- (void)close;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_WEB_VIEW_NAVIGATION_OBSERVER_H_
