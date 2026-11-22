// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_VIEW_ONLY_WK_WEB_VIEW_CONFIGURATION_UTIL_H_
#define IOS_WEB_PUBLIC_WEB_VIEW_ONLY_WK_WEB_VIEW_CONFIGURATION_UTIL_H_

@class WKWebView;
@class WKWebViewConfiguration;

namespace web {

class WebState;

// Creates the web view of `web_state` with given `configuration`.
// Returns the created web view.
// If `configuration` is nil, a new WKWebViewConfiguration object will be
// created and used to create the web view.
// This must be called immediately after `web_state` is created
// e.g., with web::WebState::Create().
//
// The goal of writing this function is to make it possible to construct
// CWVWebView from a WKWebViewConfiguration that is not originated from a
// //ios/web managed WKWebView (e.g. when handling window.open() from a normal
// WKWebView that is not //ios/web, such a WKWebViewConfiguration will be
// provided and necessary for creating a new web view).
WKWebView* EnsureWebViewCreatedWithConfiguration(
    WebState* web_state,
    WKWebViewConfiguration* configuration);

// Prepares `web_state` for its first navigation, so that it can start sooner,
// by creating the web view of `web_state` if it has not been created yet. The
// web view uses the configuration currently held by the
// WKWebViewConfigurationProvider of the `web_state`'s BrowserState, the same
// way it would be created lazily for the first navigation. Unlike
// `EnsureWebViewCreatedWithConfiguration`, this does not reset that provider.
// Nothing is loaded, so no navigation item is created, and no //ios/web
// JavaScript feature or web frame is involved.
//
// Does nothing if web usage is disabled for `web_state` or if its web view
// already exists (for example, once a navigation has started). Calling it more
// than once is safe.
void PrewarmWebView(WebState* web_state);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_VIEW_ONLY_WK_WEB_VIEW_CONFIGURATION_UTIL_H_
