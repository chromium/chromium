// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/webui/user_actions_ui.h"

#import "components/grit/dev_ui_components_resources.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/webui/user_actions_handler.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UserActionsUI::UserActionsUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web_ui->AddMessageHandler(std::make_unique<UserActionsHandler>());

  // Set up the chrome://user-actions/ source.
  web::WebUIIOSDataSource* html_source =
      web::WebUIIOSDataSource::Create(kChromeUIUserActionsHost);
  html_source->SetDefaultResource(IDR_USER_ACTIONS_HTML);
  html_source->AddResourcePath("user_actions.css", IDR_USER_ACTIONS_CSS);
  html_source->AddResourcePath("user_actions.js", IDR_USER_ACTIONS_JS);
  web::WebUIIOSDataSource::Add(ChromeBrowserState::FromWebUIIOS(web_ui),
                               html_source);
}

UserActionsUI::~UserActionsUI() {}
