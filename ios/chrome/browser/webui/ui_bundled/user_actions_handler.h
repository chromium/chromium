// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_USER_ACTIONS_HANDLER_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_USER_ACTIONS_HANDLER_H_

#include <string>

#include "base/metrics/user_metrics.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

namespace base {
class TimeTicks;
}  // namespace base

// UI Handler for chrome://user-actions/
// It listens to user action notifications and passes those notifications
// into the Javascript to update the page.
class UserActionsHandler : public web::WebUIIOSMessageHandler {
 public:
  UserActionsHandler();

  UserActionsHandler(const UserActionsHandler&) = delete;
  UserActionsHandler& operator=(const UserActionsHandler&) = delete;

  ~UserActionsHandler() override;

  // WebUIIOSMessageHandler.
  void RegisterMessages() override;

 private:
  // Called whenever a user action is registered.
  void OnUserAction(const std::string& action, base::TimeTicks action_time);

  // The callback to invoke whenever a user action is registered.
  base::ActionCallback action_callback_;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_USER_ACTIONS_HANDLER_H_
