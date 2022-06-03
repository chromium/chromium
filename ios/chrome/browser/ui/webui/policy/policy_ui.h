// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_POLICY_POLICY_UI_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_POLICY_POLICY_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {
class WebUIIOS;
}

// The Web UI controller for the chrome://policy page.
class PolicyUI : public web::WebUIIOSController {
 public:
  explicit PolicyUI(web::WebUIIOS* web_ui, const std::string& host);
  ~PolicyUI() override;
  PolicyUI(const PolicyUI&) = delete;
  PolicyUI& operator=(const PolicyUI&) = delete;
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_POLICY_POLICY_UI_H_
