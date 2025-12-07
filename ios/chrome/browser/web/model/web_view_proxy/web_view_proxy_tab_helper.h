// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_WEB_VIEW_PROXY_WEB_VIEW_PROXY_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_WEB_VIEW_PROXY_WEB_VIEW_PROXY_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#import "base/observer_list.h"
#import "base/observer_list_types.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

// WebStateUserData that provides access to this tab's CRWWebViewProxy i.e.
// either the default `web_state_->GetWebViewProxy()` or an overriding
// CRWWebViewProxy.
class WebViewProxyTabHelper
    : public web::WebStateUserData<WebViewProxyTabHelper> {
 public:
  // Observer for WebViewProxyTabHelper.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the tab's CRWWebViewProxy is changed.
    virtual void WebViewProxyChanged(WebViewProxyTabHelper* tab_helper) = 0;
    // Called when the tab helper is being destroyed.
    virtual void WebViewProxyTabHelperDestroyed(
        WebViewProxyTabHelper* tab_helper) = 0;
  };

  explicit WebViewProxyTabHelper(web::WebState* web_state);
  ~WebViewProxyTabHelper() override;

  // Returns the tab's overriding CRWWebViewProxy.
  id<CRWWebViewProxy> GetWebViewProxy() const;
  // Set the tab's overriding CRWWebViewProxy.
  void SetOverridingWebViewProxy(id<CRWWebViewProxy> web_view_proxy);

  // Add/remove an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class web::WebStateUserData<WebViewProxyTabHelper>;

  __weak id<CRWWebViewProxy> overriding_web_view_proxy_ = nil;
  raw_ptr<web::WebState> web_state_;
  base::ObserverList<Observer, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_WEB_VIEW_PROXY_WEB_VIEW_PROXY_TAB_HELPER_H_
