// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_MANAGEMENT_MANAGEMENT_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_MANAGEMENT_MANAGEMENT_UI_H_

#include <string>

#import "ios/web/public/webui/web_ui_ios_controller.h"

// The WebUI handler for chrome://management which displays the details about
// the current enterprise management state.
class ManagementUI : public web::WebUIIOSController {
 public:
  explicit ManagementUI(web::WebUIIOS* web_ui, const std::string& host);
  ~ManagementUI() override;

 private:
  ManagementUI(const ManagementUI&) = delete;
  ManagementUI& operator=(const ManagementUI&) = delete;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_MANAGEMENT_MANAGEMENT_UI_H_
