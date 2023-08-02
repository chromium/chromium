// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_view_web_state_map.h"

#import "base/memory/ptr_util.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"

namespace {
const char kWebViewWebStateMapKeyName[] = "web_view_web_state_map";
}  // namespace

namespace web {

WebViewWebStateMap::WebViewWebStateMap(BrowserState* browser_state) {}

WebViewWebStateMap::~WebViewWebStateMap() {
  for (const WebViewWebStateAssociation& association : mappings_) {
    association.web_state->RemoveObserver(this);
  }
}

WebViewWebStateMap* WebViewWebStateMap::FromBrowserState(
    BrowserState* browser_state) {
  DCHECK(browser_state);

  WebViewWebStateMap* web_view_web_state_map = static_cast<WebViewWebStateMap*>(
      browser_state->GetUserData(kWebViewWebStateMapKeyName));
  if (!web_view_web_state_map) {
    web_view_web_state_map = new WebViewWebStateMap(browser_state);
    browser_state->SetUserData(kWebViewWebStateMapKeyName,
                               base::WrapUnique(web_view_web_state_map));
  }
  return web_view_web_state_map;
}

void WebViewWebStateMap::SetAssociatedWebViewForWebState(WKWebView* web_view,
                                                         WebState* web_state) {
  DCHECK(web_state);

  auto it = mappings_.begin();
  while (it != mappings_.end()) {
    if (it->web_state == web_state) {
      if (web_view) {
        // Update existing entry with new webview.
        it->web_view = web_view;
      } else {
        // Remove mapping since no web view is assocaited.
        mappings_.erase(it);
        web_state->RemoveObserver(this);
      }
      return;
    }
    it++;
  }

  web_state->AddObserver(this);
  mappings_.push_back(WebViewWebStateAssociation(web_view, web_state));
}

WebState* WebViewWebStateMap::GetWebStateForWebView(WKWebView* web_view) {
  for (const WebViewWebStateAssociation& association : mappings_) {
    if (association.web_view == web_view) {
      return association.web_state;
    }
  }
  return nullptr;
}

void WebViewWebStateMap::WebStateDestroyed(WebState* web_state) {
  auto it = mappings_.begin();
  while (it != mappings_.end()) {
    if (it->web_state == web_state) {
      mappings_.erase(it);
      web_state->RemoveObserver(this);
      break;
    }
    it++;
  }
}

WebViewWebStateMap::WebViewWebStateAssociation::WebViewWebStateAssociation(
    WKWebView* web_view,
    WebState* web_state)
    : web_view(web_view), web_state(web_state) {}
WebViewWebStateMap::WebViewWebStateAssociation::~WebViewWebStateAssociation() =
    default;

}  // namespace web
