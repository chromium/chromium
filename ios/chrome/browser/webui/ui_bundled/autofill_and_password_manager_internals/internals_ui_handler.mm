// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/autofill_and_password_manager_internals/internals_ui_handler.h"

#import <optional>

#import "components/application_locale_storage/application_locale_storage.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#import "components/autofill/core/browser/logging/log_router.h"
#import "components/autofill/core/common/logging/log_buffer.h"
#import "components/grit/autofill_and_password_manager_internals_resources.h"
#import "components/grit/autofill_and_password_manager_internals_resources_map.h"
#import "components/version_info/version_info.h"
#import "components/webui/version/version_handler_helper.h"
#import "components/webui/version/version_ui_constants.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"

using autofill::LogRouter;

namespace autofill {

web::WebUIIOSDataSource* CreateInternalsHTMLSource(
    const std::string& source_name) {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(source_name);

  source->AddResourcePaths(kAutofillAndPasswordManagerInternalsResources);
  source->AddResourcePath(
      "",
      IDR_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_HTML);
  // Data strings:
  source->AddString(version_ui::kVersion,
                    std::string(version_info::GetVersionNumber()));
  source->AddString(version_ui::kOfficial, version_info::IsOfficialBuild()
                                               ? "official"
                                               : "Developer build");
  source->AddString(version_ui::kVersionModifier,
                    std::string(GetChannelString(GetChannel())));
  source->AddString(version_ui::kCL,
                    std::string(version_info::GetLastChange()));
  source->AddString(version_ui::kUserAgent, web::GetWebClient()->GetUserAgent(
                                                web::UserAgentType::MOBILE));
  source->AddString(
      "app_locale",
      GetApplicationContext()->GetApplicationLocaleStorage()->Get());
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
  web_ui()->RegisterMessageCallback(
      "dumpAddresses", base::BindRepeating(&InternalsUIHandler::OnDumpAddresses,
                                           base::Unretained(this)));
}

void InternalsUIHandler::OnLoaded(const base::Value::List& args) {
  base::Value load_event(call_on_load_);
  base::Value load_arg(false);
  base::ValueView load_args[] = {load_event, load_arg};
  web_ui()->CallJavascriptFunction("cr.webUIListenerCallback", load_args);

  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui());
  base::Value is_incognito(profile->IsOffTheRecord());
  base::Value incognito_event("notify-about-incognito");
  base::ValueView incognito_args[] = {incognito_event, is_incognito};
  web_ui()->CallJavascriptFunction("cr.webUIListenerCallback", incognito_args);

  base::Value variations_event("notify-about-variations");
  base::Value::List variations_list = version_ui::GetVariationsList();
  base::ValueView variations_args[] = {variations_event, variations_list};
  web_ui()->CallJavascriptFunction("cr.webUIListenerCallback", variations_args);
  StartSubscription();
}

void InternalsUIHandler::OnDumpAddresses(const base::Value::List& args) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui());
  PersonalDataManager* pdm = PersonalDataManagerFactory::GetForProfile(profile);
  if (!pdm) {
    return;
  }
  LogBuffer log;
  for (const AutofillProfile* address :
       pdm->address_data_manager().GetProfiles()) {
    log << *address;
  }
  if (std::optional<base::Value::Dict> result = log.RetrieveResult()) {
    LogEntry(*result);
  }
}

void InternalsUIHandler::StartSubscription() {
  LogRouter* log_router =
      get_log_router_function_.Run(ProfileIOS::FromWebUIIOS(web_ui()));
  if (!log_router) {
    return;
  }

  registered_with_log_router_ = true;
  log_router->RegisterReceiver(this);
}

void InternalsUIHandler::EndSubscription() {
  if (!registered_with_log_router_) {
    return;
  }
  registered_with_log_router_ = false;
  LogRouter* log_router =
      get_log_router_function_.Run(ProfileIOS::FromWebUIIOS(web_ui()));
  if (log_router) {
    log_router->UnregisterReceiver(this);
  }
}

void InternalsUIHandler::LogEntry(const base::Value::Dict& entry) {
  if (!registered_with_log_router_) {
    return;
  }

  base::Value log_event("add-structured-log");
  base::ValueView args[] = {log_event, entry};
  web_ui()->CallJavascriptFunction("cr.webUIListenerCallback", args);
}

}  //  namespace autofill
