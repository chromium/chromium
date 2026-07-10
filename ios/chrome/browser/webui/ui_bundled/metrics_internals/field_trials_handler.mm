// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/metrics_internals/field_trials_handler.h"

#import <string_view>

#import "base/functional/bind.h"
#import "base/values.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/variations/service/variations_service.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/webui/web_ui_ios.h"

// LINT.IfChange(field_trials_handler)

FieldTrialsHandler::FieldTrialsHandler(ProfileIOS* profile)
    : profile_(profile),
      base_handler_(std::make_unique<metrics::FieldTrialsHandlerBase>(
          this,
          GetApplicationContext()->GetVariationsService(),
          GetApplicationContext()->GetLocalState())) {}

FieldTrialsHandler::~FieldTrialsHandler() = default;

void FieldTrialsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "fetchTrialState",
      base::BindRepeating(&FieldTrialsHandler::HandleFetchState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setTrialEnrollState",
      base::BindRepeating(&FieldTrialsHandler::HandleSetEnrollState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "restart", base::BindRepeating(&FieldTrialsHandler::HandleRestart,
                                     base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "lookupTrialOrGroupName",
      base::BindRepeating(&FieldTrialsHandler::HandleLookupTrialOrGroupName,
                          base::Unretained(this)));
}

void FieldTrialsHandler::ResolvePageCallback(const base::ValueView callback_id,
                                             const base::ValueView response) {
  web_ui()->ResolveJavascriptCallback(callback_id, response);
}

void FieldTrialsHandler::HandleFetchState(const base::ListValue& args) {
  CHECK_EQ(args.size(), 1U);
  base_handler_->HandleFetchState(args[0], GetShowNames());
}

void FieldTrialsHandler::HandleSetEnrollState(const base::ListValue& args) {
  CHECK_EQ(args.size(), 4U);
  base_handler_->HandleSetEnrollState(args[0], args[1].GetString(),
                                      args[2].GetString(), args[3].GetBool());
}

void FieldTrialsHandler::HandleRestart(const base::ListValue& args) {
  // Restart is not supported natively on iOS.
}

void FieldTrialsHandler::HandleLookupTrialOrGroupName(
    const base::ListValue& args) {
  CHECK_EQ(args.size(), 2U);
  base_handler_->HandleLookupTrialOrGroupName(args[0], args[1].GetString());
}

bool FieldTrialsHandler::GetShowNames() {
  bool always_show_names =
#if defined(OFFICIAL_BUILD)
      false;
#else
      true;
#endif

  return always_show_names ||
         gaia::IsGoogleInternalAccountEmail(
             IdentityManagerFactory::GetForProfile(profile_)
                 ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                 .email);
}

// LINT.ThenChange(//chrome/browser/ui/webui/metrics_internals/field_trials_handler.cc)
