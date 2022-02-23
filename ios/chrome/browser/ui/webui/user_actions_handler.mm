// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/user_actions_handler.h"

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "base/values.h"
#include "ios/web/public/webui/web_ui_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  std::vector<const base::Value*> args{&event_name, &user_action_name};
  web_ui()->CallJavascriptFunction("cr.webUIListenerCallback", args);
}
