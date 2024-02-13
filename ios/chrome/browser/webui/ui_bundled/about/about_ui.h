// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_ABOUT_ABOUT_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_ABOUT_ABOUT_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {
class WebUIIOS;
}

// The WebUI controller for chrome://chrome-urls, chrome://histograms,
// and chrome://credits.
class AboutUI : public web::WebUIIOSController {
 public:
  explicit AboutUI(web::WebUIIOS* web_ui, const std::string& name);

  AboutUI(const AboutUI&) = delete;
  AboutUI& operator=(const AboutUI&) = delete;

  ~AboutUI() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_ABOUT_ABOUT_UI_H_
