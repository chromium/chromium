// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_disabled_browser_agent.h"

#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_signout_continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

SigninDisabledBrowserAgent::SigninDisabledBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  CHECK_EQ(browser_->type(), Browser::Type::kRegular);
  authentication_service_ =
      AuthenticationServiceFactory::GetForProfile(browser_->GetProfile());
  CHECK(authentication_service_);
  PrefService* local_pref_service = GetApplicationContext()->GetLocalState();
  local_pref_change_registrar_.Init(local_pref_service);
  local_pref_change_registrar_.Add(
      prefs::kSigninAllowedOnDevice,
      base::BindRepeating(&SigninDisabledBrowserAgent::SignOutIfNeeded,
                          base::Unretained(this)));
  SignOutIfNeeded();
}

SigninDisabledBrowserAgent::~SigninDisabledBrowserAgent() = default;

void SigninDisabledBrowserAgent::SignOutIfNeeded() {
  PrefService* local_pref_service = GetApplicationContext()->GetLocalState();
  if (local_pref_service->GetBoolean(prefs::kSigninAllowedOnDevice)) {
    return;
  }
  ProfileIOS* profile = browser_->GetProfile();
  if (!IsPersonalProfile(profile)) {
    ChangeProfileContinuation continuation =
        CreateChangeProfileSignoutContinuation(
            signin_metrics::ProfileSignout::kPrefChanged,
            /*force_snackbar_over_toolbar=*/false,
            /*should_record_metrics=*/false,
            /*snackbar_message_builder=*/{}, base::DoNothing());
    signin::SwitchToPersonalProfile(browser_->GetSceneState(),
                                    ChangeProfileReason::kManagedAccountSignOut,
                                    std::move(continuation));
    return;
  }
  if (authentication_service_->HasPrimaryIdentity()) {
    authentication_service_->SignOut(
        signin_metrics::ProfileSignout::kPrefChanged);
  }
}
