// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webui/user_actions/user_actions_ui.h"

#include <memory>

#include "components/grit/user_actions_ui_resources.h"
#include "components/grit/user_actions_ui_resources_map.h"
#include "components/webui/user_actions/user_actions_ui_constants.h"
#include "components/webui/user_actions/user_actions_ui_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

namespace user_actions_ui {
UserActionsUI::UserActionsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://user-actions/ source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          user_actions_ui::kUserActionsWebUIHost);
  html_source->AddResourcePaths(kUserActionsUiResources);
  html_source->SetDefaultResource(IDR_USER_ACTIONS_UI_USER_ACTIONS_HTML);

  web_ui->AddMessageHandler(std::make_unique<UserActionsUIHandler>());
}

UserActionsUI::~UserActionsUI() = default;
}  // namespace user_actions_ui
