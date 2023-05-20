// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/webui/autofill_and_password_manager_internals/autofill_internals_ui_ios.h"

#import "ios/chrome/browser/autofill/autofill_log_router_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/ui/webui/autofill_and_password_manager_internals/internals_ui_handler.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::LogRouter;

AutofillInternalsUIIOS::AutofillInternalsUIIOS(web::WebUIIOS* web_ui,
                                               const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  ChromeBrowserState* browser_state = ChromeBrowserState::FromWebUIIOS(web_ui);
  web::WebUIIOSDataSource::Add(
      browser_state,
      autofill::CreateInternalsHTMLSource(kChromeUIAutofillInternalsHost));
  web_ui->AddMessageHandler(std::make_unique<autofill::InternalsUIHandler>(
      "setup-autofill-internals",
      base::BindRepeating(
          &autofill::AutofillLogRouterFactory::GetForBrowserState)));
}

AutofillInternalsUIIOS::~AutofillInternalsUIIOS() = default;
