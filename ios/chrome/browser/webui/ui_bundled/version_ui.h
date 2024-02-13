// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_VERSION_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_VERSION_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

// The WebUI handler for chrome://version.
class VersionUI : public web::WebUIIOSController {
 public:
  explicit VersionUI(web::WebUIIOS* web_ui, const std::string& host);

  VersionUI(const VersionUI&) = delete;
  VersionUI& operator=(const VersionUI&) = delete;

  ~VersionUI() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_VERSION_UI_H_
