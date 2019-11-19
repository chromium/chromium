// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_VIEW_INTERNAL_CREATION_UTIL_H_
#define IOS_WEB_WEB_STATE_WEB_VIEW_INTERNAL_CREATION_UTIL_H_

#import <CoreGraphics/CoreGraphics.h>
#import <WebKit/WebKit.h>

#include "ios/web/common/user_agent.h"

@protocol CRWContextMenuDelegate;

// This file is a collection of functions that vend web views.
namespace web {
class BrowserState;

// Creates a new WKWebView for displaying regular web content and registers a
// user agent for it.
//
// Preconditions for creation of a WKWebView:
// 1) |browser_state|, |configuration| are not null.
// 2) web::BrowsingDataPartition is synchronized.
// 3) The WKProcessPool of the configuration is the same as the WKProcessPool
//    of the WKWebViewConfiguration associated with |browser_state|.
//
WKWebView* BuildWKWebView(CGRect frame,
                          WKWebViewConfiguration* configuration,
                          BrowserState* browser_state,
                          UserAgentType user_agent_type,
                          id<CRWContextMenuDelegate> context_menu_delegate);

// Creates and returns a new WKWebView for displaying regular web content.
// The preconditions for the creation of a WKWebView are the same as the
// previous method.
WKWebView* BuildWKWebView(CGRect frame,
                          WKWebViewConfiguration* configuration,
                          BrowserState* browser_state,
                          UserAgentType user_agent_type);

// Creates and returns a new WKWebView for displaying regular web content.
// The preconditions for the creation of a WKWebView are the same as the
// previous method.
WKWebView* BuildWKWebView(CGRect frame,
                          WKWebViewConfiguration* configuration,
                          BrowserState* browser_state);

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_VIEW_INTERNAL_CREATION_UTIL_H_
