// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_OMAHA_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_OMAHA_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

class OmahaUI : public web::WebUIIOSController {
 public:
  explicit OmahaUI(web::WebUIIOS* web_ui, const std::string& host);

  OmahaUI(const OmahaUI&) = delete;
  OmahaUI& operator=(const OmahaUI&) = delete;

  ~OmahaUI() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_OMAHA_UI_H_
