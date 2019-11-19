// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_DELEGATE_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_DELEGATE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "ios/web/public/init/web_main_delegate.h"

namespace ios_web_view {

// WebView implementation of WebMainDelegate.
class WebViewWebMainDelegate : public web::WebMainDelegate {
 public:
  WebViewWebMainDelegate();
  ~WebViewWebMainDelegate() override;

  // WebMainDelegate implementation.
  void BasicStartupComplete() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewWebMainDelegate);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_DELEGATE_H_
