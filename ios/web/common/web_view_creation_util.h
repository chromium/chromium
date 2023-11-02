// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_WEB_VIEW_CREATION_UTIL_H_
#define IOS_WEB_COMMON_WEB_VIEW_CREATION_UTIL_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

@class WKWebView;

namespace web {
class BrowserState;

// Returns a new WKWebView for displaying regular web content.
// WKWebViewConfiguration object for resulting web view will be obtained from
// the given `browser_state`.
//
// Preconditions for creation of a WKWebView:
// 1) `browser_state` is not null.
// 2) web::BrowsingDataPartition is synchronized.
//
WKWebView* BuildWKWebView(CGRect frame, BrowserState* browser_state);

// Returns a new WKWebView that will not be used to display content.
// This WKWebView can be used to fetch some data using the same cookie store
// as the other WKWebView but cannot be presented to the user as some components
// are not initialized (e.g. voice search).
//
// Preconditions for creation of a WKWebView:
// 1) `browser_state` is not null.
// 2) web::BrowsingDataPartition is synchronized.
//
WKWebView* BuildWKWebViewForQueries(BrowserState* browser_state);

}  // web

#endif  // IOS_WEB_COMMON_WEB_VIEW_CREATION_UTIL_H_
