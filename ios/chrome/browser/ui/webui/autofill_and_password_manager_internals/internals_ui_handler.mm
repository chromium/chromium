// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/autofill_and_password_manager_internals/internals_ui_handler.h"

#include "components/autofill/core/browser/logging/log_router.h"
#include "components/grit/components_resources.h"
#include "components/version_info/version_info.h"
#include "components/version_ui/version_handler_helper.h"
#include "components/version_ui/version_ui_constants.h"
#import "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::LogRouter;

namespace autofill {

web::WebUIIOSDataSource* CreateInternalsHTMLSource(
    const std::string& source_name) {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(source_name);

  source->AddResourcePath("autofill_and_password_manager_internals.js",
                          IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_JS);
  source->AddResourcePath("autofill_and_password_manager_internals.css",
                          IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_CSS);
  source->SetDefaultResource(IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_HTML);
  // Data strings:
  source->AddString(version_ui::kVersion, version_info::GetVersionNumber());
  source->AddString(version_ui::kOfficial, version_info::IsOfficialBuild()
                                               ? "official"
                                               : "Developer build");
  source->AddString(version_ui::kVersionModifier,
                    GetChannelString(GetChannel()));
  source->AddString(version_ui::kCL, version_info::GetLastChange());
  source->AddString(version_ui::kUserAgent, web::GetWebClient()->GetUserAgent(
                                                web::UserAgentType::MOBILE));
  source->AddString("app_locale",
                    GetApplicationContext()->GetApplicationLocale());
  return source;
}

InternalsUIHandler::InternalsUIHandler(
    std::string call_on_load,
    GetLogRouterFunction get_log_router_function)
    : call_on_load_(std::move(call_on_load)),
      get_log_router_function_(std::move(get_log_router_function)) {}

InternalsUIHandler::~InternalsUIHandler() {
  EndSubscription();
}

void InternalsUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "loaded", base::BindRepeating(&InternalsUIHandler::OnLoaded,
                                    base::Unretained(this)));
}

void InternalsUIHandler::OnLoaded(const base::ListValue* args) {
  web_ui()->CallJavascriptFunction(call_on_load_, {});

  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromWebUIIOS(web_ui());
  base::Value is_incognito(browser_state->IsOffTheRecord());
  web_ui()->CallJavascriptFunction("notifyAboutIncognito", {&is_incognito});
  web_ui()->CallJavascriptFunction("notifyAboutVariations",
                                   {version_ui::GetVariationsList().get()});
  StartSubscription();
}

void InternalsUIHandler::StartSubscription() {
  LogRouter* log_router = get_log_router_function_.Run(
      ios::ChromeBrowserState::FromWebUIIOS(web_ui()));
  if (!log_router)
    return;

  registered_with_log_router_ = true;

  const auto& past_logs = log_router->RegisterReceiver(this);
  for (const auto& entry : past_logs)
    LogEntry(entry);
}

void InternalsUIHandler::EndSubscription() {
  if (!registered_with_log_router_)
    return;
  registered_with_log_router_ = false;
  LogRouter* log_router = get_log_router_function_.Run(
      ios::ChromeBrowserState::FromWebUIIOS(web_ui()));
  if (log_router)
    log_router->UnregisterReceiver(this);
}

void InternalsUIHandler::LogEntry(const base::Value& entry) {
  if (!registered_with_log_router_ || entry.is_none())
    return;
  web_ui()->CallJavascriptFunction("addRawLog", {&entry});
}

}  //  namespace autofill
