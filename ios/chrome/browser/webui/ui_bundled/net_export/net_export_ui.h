// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_NET_EXPORT_NET_EXPORT_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_NET_EXPORT_NET_EXPORT_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

// The C++ back-end for the chrome://net-export webui page.
class NetExportUI : public web::WebUIIOSController {
 public:
  explicit NetExportUI(web::WebUIIOS* web_ui, const std::string& host);

  NetExportUI(const NetExportUI&) = delete;
  NetExportUI& operator=(const NetExportUI&) = delete;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_NET_EXPORT_NET_EXPORT_UI_H_
