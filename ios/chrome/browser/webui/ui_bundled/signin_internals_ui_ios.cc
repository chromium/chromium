// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/webui/ui_bundled/signin_internals_ui_ios.h"

#include <optional>
#include <string>
#include <vector>

#include "base/hash/hash.h"
#include "components/grit/signin_internals_resources.h"
#include "components/grit/signin_internals_resources_map.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/version_info/channel.h"
#include "google_apis/gaia/core_account_id.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#include "ios/chrome/browser/signin/model/about_signin_internals_factory.h"
#include "ios/chrome/browser/signin/model/identity_manager_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"

namespace {

web::WebUIIOSDataSource* CreateSignInInternalsHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUISignInInternalsHost);

  source->UseStringsJs();
  source->AddResourcePaths(kSigninInternalsResources);
  source->AddResourcePath("", IDR_SIGNIN_INTERNALS_SIGNIN_INDEX_HTML);

  return source;
}

}  //  namespace

SignInInternalsUIIOS::SignInInternalsUIIOS(web::WebUIIOS* web_ui,
                                           const std::string& host)
    : WebUIIOSController(web_ui, host) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui);
  DCHECK(profile);
  web::WebUIIOSDataSource::Add(profile, CreateSignInInternalsHTMLSource());
  web_ui->AddMessageHandler(std::make_unique<SignInInternalsHandlerIOS>());
}

SignInInternalsUIIOS::~SignInInternalsUIIOS() = default;

SignInInternalsHandlerIOS::SignInInternalsHandlerIOS() {}

SignInInternalsHandlerIOS::~SignInInternalsHandlerIOS() = default;

void SignInInternalsHandlerIOS::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getSigninInfo",
      base::BindRepeating(&SignInInternalsHandlerIOS::HandleGetSignInInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "overrideCapability",
      base::BindRepeating(&SignInInternalsHandlerIOS::HandleOverrideCapability,
                          base::Unretained(this)));
}

void SignInInternalsHandlerIOS::HandleOverrideCapability(
    const base::ListValue& args) {
  if (args.size() != 3u) {
    return;
  }

  const std::string* account_id_str = args[0].GetIfString();
  const std::string* capability_name = args[1].GetIfString();
  const std::string* value_str = args[2].GetIfString();

  if (!account_id_str || !capability_name || !value_str ||
      account_id_str->empty() || capability_name->empty()) {
    return;
  }

  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui());
  if (!profile) {
    return;
  }

  AboutSigninInternals* about_signin_internals =
      ios::AboutSigninInternalsFactory::GetForProfile(profile);
  if (!about_signin_internals) {
    return;
  }

  CoreAccountId account_id = CoreAccountId::FromString(*account_id_str);
  if (!about_signin_internals->CanOverrideAccountCapability(
          account_id, *capability_name, ::GetChannel())) {
    return;
  }

  std::optional<signin::Tribool> override_value;
  if (*value_str == "True") {
    override_value = signin::Tribool::kTrue;
  } else if (*value_str == "False") {
    override_value = signin::Tribool::kFalse;
  } else if (*value_str == "Unknown") {
    override_value = signin::Tribool::kUnknown;
  } else if (value_str->empty()) {
    override_value = std::nullopt;
  } else {
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return;
  }

  identity_manager->SetCapabilityOverride(account_id, *capability_name,
                                          override_value);
}

void SignInInternalsHandlerIOS::HandleGetSignInInfo(
    const base::ListValue& args) {
  CHECK_GE(args.size(), 1u);
  // CHECKs if non-string.
  const std::string& callback_id = args[0].GetString();
  base::Value callback(callback_id);
  base::Value success(true);

  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui());
  DCHECK(profile);
  AboutSigninInternals* about_signin_internals =
      ios::AboutSigninInternalsFactory::GetForProfile(profile);

  if (!about_signin_internals) {
    base::Value empty;
    base::ValueView return_args[] = {callback, success, empty};
    web_ui()->CallJavascriptFunction("cr.webUIResponse", return_args);
    return;
  }

  // Note(vishwath): The UI would look better if we passed in a dict with some
  // reasonable defaults, so the about:signin-internals page doesn't look
  // empty in incognito mode. Alternatively, we could force about:signin to
  // open in non-incognito mode always (like about:settings for ex.).
  if (!about_signin_internals_observation_.IsObserving()) {
    about_signin_internals_observation_.Observe(about_signin_internals);
  }
  base::DictValue status = about_signin_internals->GetSigninStatus();
  AddAccountCapabilitiesOverridesInfo(status);
  base::ValueView return_args[] = {callback, success, status};
  web_ui()->CallJavascriptFunction("cr.webUIResponse", return_args);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar =
      identity_manager->GetAccountsInCookieJar();
  if (accounts_in_cookie_jar.AreAccountsFresh()) {
    about_signin_internals->OnAccountsInCookieUpdated(
        accounts_in_cookie_jar, GoogleServiceAuthError::AuthErrorNone());
  }
}

void SignInInternalsHandlerIOS::OnSigninStateChanged(
    const base::DictValue& info) {
  base::DictValue status = info.Clone();
  AddAccountCapabilitiesOverridesInfo(status);
  base::Value event_name("signin-info-changed");
  base::ValueView args[] = {event_name, status};
  web_ui()->CallJavascriptFunction("cr.webUIListenerCallback", args);
}

void SignInInternalsHandlerIOS::OnCookieAccountsFetched(
    const base::DictValue& info) {
  base::Value event_name("update-cookie-accounts");
  base::ValueView args[] = {event_name, info};
  web_ui()->CallJavascriptFunction("cr.webUIListenerCallback", args);
}

void SignInInternalsHandlerIOS::AddAccountCapabilitiesOverridesInfo(
    base::DictValue& signin_status) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui());
  if (!profile) {
    return;
  }
  AboutSigninInternals* about_signin_internals =
      ios::AboutSigninInternalsFactory::GetForProfile(profile);
  if (!about_signin_internals) {
    return;
  }

  about_signin_internals->AddAccountCapabilitiesOverridesInfo(signin_status,
                                                              ::GetChannel());
}
