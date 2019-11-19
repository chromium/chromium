// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_WEB_VIEW_CREATION_UTIL_H_
#define IOS_WEB_COMMON_WEB_VIEW_CREATION_UTIL_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

@protocol CRWContextMenuDelegate;
@class WKWebView;

namespace web {
class BrowserState;

// Returns a new WKWebView for displaying regular web content.
// WKWebViewConfiguration object for resulting web view will be obtained from
// the given |browser_state|.
//
// Preconditions for creation of a WKWebView:
// 1) |browser_state| is not null.
// 2) web::BrowsingDataPartition is synchronized.
//
WKWebView* BuildWKWebView(CGRect frame, BrowserState* browser_state);

// Returns a new WKWebView for displaying regular web content.
// The returned WKWebView is equivalent to the one created by |BuildWKWebView|
// but a context menu recognizer is attached to it.
// On a long press, context_menu_delegate webView:handleContextMenu:| is called.
// The custom context menu involves gesture recognizers on every touch and
// JavaScript. It can have impact on performances.
// Calling |BuildWKWebViewWithCustomContextMenu| with a |context_menu_delegate|
// nil is equivalent to |BuildWKWebView|.
WKWebView* BuildWKWebViewWithCustomContextMenu(
    CGRect frame,
    BrowserState* browser_state,
    id<CRWContextMenuDelegate> context_menu_delegate);

}  // web

#endif  // IOS_WEB_COMMON_WEB_VIEW_CREATION_UTIL_H_
