// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_WEB_VIEW_WEB_STATE_MAP_H_
#define IOS_WEB_JS_MESSAGING_WEB_VIEW_WEB_STATE_MAP_H_

#include <list>

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "ios/web/public/web_state_observer.h"

@class WKWebView;

namespace web {

class BrowserState;
class WebState;

// Maps WKWebView to WebStates associated with `browser_state`. Mappings will
// only be stored for WebStates which currently have a WKWebView. This class
// allows for obtaining the correct WebState when only a WKWebView is known. For
// example, to correctly route WKScriptMessage responses to the origin WebState.
class WebViewWebStateMap : public base::SupportsUserData::Data,
                           public WebStateObserver {
 public:
  // Returns the WebViewWebStateMap associated with `browser_state`, creating
  // one if necessary. `browser_state` cannot be a null.
  static WebViewWebStateMap* FromBrowserState(BrowserState* browser_state);

  // Sets the WebState associated with `web_view`. `web_state` must not be null,
  // `web_view` may be null to remove any stored association for `web_state`.
  void SetAssociatedWebViewForWebState(WKWebView* web_view,
                                       WebState* web_state);

  // Returns the WebState which owns `web_view`, if and only if any of the live
  // WebStates associated with `browser_state` own `web_view`. Otherwise,
  // returns null.
  WebState* GetWebStateForWebView(WKWebView* web_view);

  WebViewWebStateMap(const WebViewWebStateMap&) = delete;
  WebViewWebStateMap& operator=(const WebViewWebStateMap&) = delete;

  explicit WebViewWebStateMap(BrowserState* browser_state);
  ~WebViewWebStateMap() override;

 private:
  // WebStateObserver:
  void WebStateDestroyed(WebState* web_state) override;

  // Associates a WebState with a WKWebView.
  struct WebViewWebStateAssociation {
    WebViewWebStateAssociation(WKWebView* web_view, WebState* web_state);
    ~WebViewWebStateAssociation();
    WebViewWebStateAssociation(const WebViewWebStateAssociation&) = default;
    __weak WKWebView* web_view;
    raw_ptr<WebState> web_state;
  };
  std::list<WebViewWebStateAssociation> mappings_;
};

}  // namespace web
#endif  // IOS_WEB_JS_MESSAGING_WEB_VIEW_WEB_STATE_MAP_H_
