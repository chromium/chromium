// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_profile_info_updater.h"

#import <string>

#import "base/strings/utf_string_conversions.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

SigninProfileInfoUpdater::SigninProfileInfoUpdater(
    signin::IdentityManager* identity_manager,
    SigninErrorController* signin_error_controller,
    const std::string& profile_name)
    : identity_manager_(identity_manager),
      signin_error_controller_(signin_error_controller),
      profile_name_(profile_name) {
  DCHECK(GetApplicationContext()->GetProfileManager());
  identity_manager_observation_.Observe(identity_manager_.get());

  signin_error_controller_observation_.Observe(signin_error_controller);

  UpdateProfileInfo();
  // TODO(crbug.com/40603806): Call OnErrorChanged() here, to catch any change
  // that happened since the construction of SigninErrorController. BrowserState
  // metrics depend on this bug and must be fixed first.
}

SigninProfileInfoUpdater::~SigninProfileInfoUpdater() = default;

void SigninProfileInfoUpdater::Shutdown() {
  identity_manager_observation_.Reset();
  signin_error_controller_observation_.Reset();
}

void SigninProfileInfoUpdater::UpdateProfileInfo() {
  GetApplicationContext()
      ->GetProfileManager()
      ->GetProfileAttributesStorage()
      ->UpdateAttributesForProfileWithName(
          profile_name_,
          base::BindOnce(
              [](CoreAccountInfo info, ProfileAttributesIOS& attr) {
                attr.SetAuthenticationInfo(info.gaia, info.email);
              },
              identity_manager_->GetPrimaryAccountInfo(
                  signin::ConsentLevel::kSignin)));
}

void SigninProfileInfoUpdater::UpdateWidgetsInfo(
    const signin::PrimaryAccountChangeEvent& event) {
  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
  const std::string& current_gaia_id =
      event.GetCurrentState().primary_account.gaia.ToString();
  [shared_defaults setObject:base::SysUTF8ToNSString(current_gaia_id)
                      forKey:app_group::kPrimaryAccount];
}

void SigninProfileInfoUpdater::OnErrorChanged() {
  GetApplicationContext()
      ->GetProfileManager()
      ->GetProfileAttributesStorage()
      ->UpdateAttributesForProfileWithName(
          profile_name_, base::BindOnce(
                             [](bool has_error, ProfileAttributesIOS& attr) {
                               attr.SetHasAuthenticationError(has_error);
                             },
                             signin_error_controller_->HasError()));
}

void SigninProfileInfoUpdater::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  UpdateProfileInfo();
  UpdateWidgetsInfo(event);
}
