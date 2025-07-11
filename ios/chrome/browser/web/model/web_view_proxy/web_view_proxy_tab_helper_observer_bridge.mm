// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper_observer_bridge.h"

WebViewProxyTabHelperObserverBridge::WebViewProxyTabHelperObserverBridge(
    id<WebViewProxyTabHelperObserving> observer)
    : observer_(observer) {}

WebViewProxyTabHelperObserverBridge::~WebViewProxyTabHelperObserverBridge() =
    default;

void WebViewProxyTabHelperObserverBridge::WebViewProxyChanged(
    WebViewProxyTabHelper* tab_helper) {
  [observer_ webViewProxyDidChange:tab_helper];
}

void WebViewProxyTabHelperObserverBridge::WebViewProxyTabHelperDestroyed(
    WebViewProxyTabHelper* tab_helper) {
  [observer_ webViewProxyTabHelperWasDestroyed:tab_helper];
}
