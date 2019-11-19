// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/autofill_and_password_manager_internals/password_manager_internals_ui_ios.h"

#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/passwords/password_manager_log_router_factory.h"
#import "ios/chrome/browser/ui/webui/autofill_and_password_manager_internals/internals_ui_handler.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::LogRouter;

PasswordManagerInternalsUIIOS::PasswordManagerInternalsUIIOS(
    web::WebUIIOS* web_ui)
    : web::WebUIIOSController(web_ui) {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromWebUIIOS(web_ui);
  web::WebUIIOSDataSource::Add(browser_state,
                               autofill::CreateInternalsHTMLSource(
                                   kChromeUIPasswordManagerInternalsHost));
  web_ui->AddMessageHandler(std::make_unique<autofill::InternalsUIHandler>(
      "setUpPasswordManagerInternals",
      base::BindRepeating(
          &ios::PasswordManagerLogRouterFactory::GetForBrowserState)));
}

PasswordManagerInternalsUIIOS::~PasswordManagerInternalsUIIOS() = default;
