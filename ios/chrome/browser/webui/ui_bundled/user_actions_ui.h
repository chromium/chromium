// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_USER_ACTIONS_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_USER_ACTIONS_UI_H_

#include <string>

#include "ios/web/public/webui/web_ui_ios_controller.h"

// The UI for chrome://user-actions/
class UserActionsUI : public web::WebUIIOSController {
 public:
  explicit UserActionsUI(web::WebUIIOS* web_ui, const std::string& host);

  UserActionsUI(const UserActionsUI&) = delete;
  UserActionsUI& operator=(const UserActionsUI&) = delete;

  ~UserActionsUI() override;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_USER_ACTIONS_UI_H_
