// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_WEB_VIEW_PROXY_WEB_VIEW_PROXY_TAB_HELPER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_WEB_VIEW_PROXY_WEB_VIEW_PROXY_TAB_HELPER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"

// Objective-C protocol for observing WebViewProxyTabHelper events.
@protocol WebViewProxyTabHelperObserving <NSObject>
- (void)webViewProxyDidChange:(WebViewProxyTabHelper*)tabHelper;
- (void)webViewProxyTabHelperWasDestroyed:(WebViewProxyTabHelper*)tabHelper;
@end

// Bridge class that forwards WebViewProxyTabHelper::Observer events to a
// Objective-C delegate.
class WebViewProxyTabHelperObserverBridge
    : public WebViewProxyTabHelper::Observer {
 public:
  WebViewProxyTabHelperObserverBridge(
      id<WebViewProxyTabHelperObserving> observer);
  ~WebViewProxyTabHelperObserverBridge() override;

  void WebViewProxyChanged(WebViewProxyTabHelper* tab_helper) override;
  void WebViewProxyTabHelperDestroyed(
      WebViewProxyTabHelper* tab_helper) override;

 private:
  __weak id<WebViewProxyTabHelperObserving> observer_;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_WEB_VIEW_PROXY_WEB_VIEW_PROXY_TAB_HELPER_OBSERVER_BRIDGE_H_
