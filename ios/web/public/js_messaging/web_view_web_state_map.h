// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_WEB_VIEW_WEB_STATE_MAP_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_WEB_VIEW_WEB_STATE_MAP_H_

@class WKWebView;

namespace web {

class BrowserState;
class WebState;

// Maps WKWebView to WebStates associated with |browser_state|. Mappings will
// only be stored for WebStates which currently have a WKWebView. This class
// allows for obtaining the correct WebState when only a WKWebView is known. For
// example, to correctly route WKScriptMessage responses to the origin WebState.
class WebViewWebStateMap {
 public:
  // Returns the WebViewWebStateMap associated with |browser_state|, creating
  // one if necessary. |browser_state| cannot be a null.
  static WebViewWebStateMap* FromBrowserState(BrowserState* browser_state);

  // Returns the WebState which owns |web_view|, if and only if any of the live
  // WebStates associated with |browser_state| own |web_view|. Otherwise,
  // returns null.
  virtual WebState* GetWebStateForWebView(WKWebView* web_view) = 0;

  WebViewWebStateMap(const WebViewWebStateMap&) = delete;
  WebViewWebStateMap& operator=(const WebViewWebStateMap&) = delete;

 protected:
  WebViewWebStateMap() = default;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_WEB_VIEW_WEB_STATE_MAP_H_
