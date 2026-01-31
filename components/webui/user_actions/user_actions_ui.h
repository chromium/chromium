// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBUI_USER_ACTIONS_USER_ACTIONS_UI_H_
#define COMPONENTS_WEBUI_USER_ACTIONS_USER_ACTIONS_UI_H_

#include "components/webui/user_actions/user_actions_ui_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/web_ui_controller.h"

namespace user_actions_ui {

// The UI for chrome://user-actions/
class UserActionsUI : public content::WebUIController {
 public:
  explicit UserActionsUI(content::WebUI* contents);

  UserActionsUI(const UserActionsUI&) = delete;
  UserActionsUI& operator=(const UserActionsUI&) = delete;

  ~UserActionsUI() override;
};

class UserActionsUIConfig
    : public content::DefaultInternalWebUIConfig<UserActionsUI> {
 public:
  UserActionsUIConfig()
      : DefaultInternalWebUIConfig(user_actions_ui::kUserActionsWebUIHost) {}
};

}  // namespace user_actions_ui

#endif  // COMPONENTS_WEBUI_USER_ACTIONS_USER_ACTIONS_UI_H_
