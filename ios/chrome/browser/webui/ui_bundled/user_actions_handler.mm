// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/user_actions_handler.h"

#import "base/functional/bind.h"
#import "base/metrics/user_metrics.h"
#import "base/time/time.h"
#import "base/values.h"
#import "ios/web/public/webui/web_ui_ios.h"

UserActionsHandler::UserActionsHandler()
    : action_callback_(base::BindRepeating(&UserActionsHandler::OnUserAction,
                                           base::Unretained(this))) {
  base::AddActionCallback(action_callback_);
}

UserActionsHandler::~UserActionsHandler() {
  base::RemoveActionCallback(action_callback_);
}

void UserActionsHandler::RegisterMessages() {}

void UserActionsHandler::OnUserAction(const std::string& action,
                                      base::TimeTicks action_time) {
  base::Value event_name = base::Value("user-action");
  base::Value user_action_name(action);
  base::ValueView args[] = {event_name, user_action_name};
  web_ui()->CallJavascriptFunction("cr.webUIListenerCallback", args);
}
