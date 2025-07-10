// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"

#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"

WebViewProxyTabHelper::WebViewProxyTabHelper(web::WebState* web_state)
    : web_state_(web_state) {}

WebViewProxyTabHelper::~WebViewProxyTabHelper() {
  for (Observer& observer : observers_) {
    observer.WebViewProxyTabHelperDestroyed(this);
  }
}

id<CRWWebViewProxy> WebViewProxyTabHelper::GetWebViewProxy() const {
  return overriding_web_view_proxy_ ?: web_state_->GetWebViewProxy();
}

void WebViewProxyTabHelper::SetOverridingWebViewProxy(
    id<CRWWebViewProxy> web_view_proxy) {
  overriding_web_view_proxy_ = web_view_proxy;
  for (Observer& observer : observers_) {
    observer.WebViewProxyChanged(this);
  }
}

void WebViewProxyTabHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WebViewProxyTabHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}
