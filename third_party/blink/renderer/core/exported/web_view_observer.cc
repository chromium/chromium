// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_view_observer.h"

#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

WebViewObserver::WebViewObserver(WebView* web_view)
    : web_view_(To<WebViewImpl>(web_view)) {
  // |web_view_| can be null on unit testing or if Observe() is used.
  if (web_view_) {
    web_view_->AddObserver(this);
  }
}

WebViewObserver::~WebViewObserver() {
  Observe(nullptr);
}

WebView* WebViewObserver::GetWebView() const {
  return web_view_;
}

void WebViewObserver::Observe(WebView* web_view) {
  if (web_view_) {
    web_view_->RemoveObserver(this);
  }

  web_view_ = To<WebViewImpl>(web_view);
  if (web_view_) {
    web_view_->AddObserver(this);
  }
}

void WebViewObserver::WebViewDestroyed() {
  Observe(nullptr);
  OnDestruct();
}

}  // namespace blink
