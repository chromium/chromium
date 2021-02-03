// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_WEB_VIEW_WEB_STATE_MAP_IMPL_H_
#define IOS_WEB_JS_MESSAGING_WEB_VIEW_WEB_STATE_MAP_IMPL_H_

#include <list>

#include "base/supports_user_data.h"
#import "ios/web/public/js_messaging/web_view_web_state_map.h"
#include "ios/web/public/web_state_observer.h"

@class WKWebView;

namespace web {

class BrowserState;
class WebState;

// A concrete implementation of WebViewWebStateMap which exposes a setter to
// store the mappings of WKWebViews to WebStates.
class WebViewWebStateMapImpl : public base::SupportsUserData::Data,
                               public WebViewWebStateMap,
                               public WebStateObserver {
 public:
  explicit WebViewWebStateMapImpl(BrowserState* browser_state);
  ~WebViewWebStateMapImpl() override;

  // Returns the WebViewWebStateMap associated with |browser_state|, creating
  // one if necessary. |browser_state| cannot be a null.
  static WebViewWebStateMapImpl* FromBrowserState(BrowserState* browser_state);

  // Sets the WebState associated with |web_view|. |web_state| must not be null,
  // |web_view| may be null to remove any stored association for |web_state|.
  void SetAssociatedWebViewForWebState(WKWebView* web_view,
                                       WebState* web_state);

  // WebViewWebStateMap:
  WebState* GetWebStateForWebView(WKWebView* web_view) override;

 private:
  // WebStateObserver:
  void WebStateDestroyed(WebState* web_state) override;

  // Associates a WebState with a WKWebView.
  struct WebViewWebStateAssociation {
    WebViewWebStateAssociation(WKWebView* web_view, WebState* web_state);
    ~WebViewWebStateAssociation();
    WebViewWebStateAssociation(const WebViewWebStateAssociation&) = default;
    __weak WKWebView* web_view;
    WebState* web_state;
  };
  std::list<WebViewWebStateAssociation> mappings_;
};

}  // namespace web
#endif  // IOS_WEB_JS_MESSAGING_WEB_VIEW_WEB_STATE_MAP_IMPL_H_
