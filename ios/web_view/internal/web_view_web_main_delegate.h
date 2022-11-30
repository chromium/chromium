// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_DELEGATE_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_DELEGATE_H_

#include "ios/web/public/init/web_main_delegate.h"

namespace ios_web_view {

// WebView implementation of WebMainDelegate.
class WebViewWebMainDelegate : public web::WebMainDelegate {
 public:
  WebViewWebMainDelegate();

  WebViewWebMainDelegate(const WebViewWebMainDelegate&) = delete;
  WebViewWebMainDelegate& operator=(const WebViewWebMainDelegate&) = delete;

  ~WebViewWebMainDelegate() override;

  // WebMainDelegate implementation.
  void BasicStartupComplete() override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_DELEGATE_H_
