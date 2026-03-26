// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_WEB_VIEW_WEB_STATE_MAP_H_
#define IOS_WEB_JS_MESSAGING_WEB_VIEW_WEB_STATE_MAP_H_

@class WKWebView;

namespace web {

class WebState;

// Sets the WebState associated with `web_view`. `web_state` and `web_view`,
// must not be null. It is possible to associate the same WebState to multiple
// WKWebView (but only one WebState can be associated per WKWebView).
void SetAssociatedWebViewForWebState(WKWebView* web_view, WebState* web_state);

// Clears the association between `web_view` and `web_state`. The mapping must
// have been created earlier by calling `SetAssociatedWebViewForWebState()`.
void ClearAssociatedWebViewForWebState(WKWebView* web_view,
                                       WebState* web_state);

// Returns the WebState associated with `web_view` by a previous call to
// `SetAssociatedWebViewForWebState()`. Will return null if there is no
// such mapping.
WebState* GetWebStateForWebView(WKWebView* web_view);

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_WEB_VIEW_WEB_STATE_MAP_H_
