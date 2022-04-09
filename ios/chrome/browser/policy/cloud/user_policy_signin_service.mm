// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/cloud/user_policy_signin_service.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "components/policy/core/browser/cloud/user_policy_signin_service_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/policy/cloud/user_policy_switch.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// TODO(crbug.com/1312552): Move
// chrome/browser/signin/account_id_from_account_info.h to components/ to be
// able to reuse the helper here.
//
// Gets the AccountId from the provided |account_info|.
AccountId AccountIdFromAccountInfo(const CoreAccountInfo& account_info) {
  if (account_info.email.empty() || account_info.gaia.empty())
    return EmptyAccountId();

  return AccountId::FromUserEmailGaiaId(
      gaia::CanonicalizeEmail(account_info.email), account_info.gaia);
}

}  // namespace

namespace policy {

UserPolicySigninService::UserPolicySigninService(
    PrefService* browser_state_prefs,
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    UserCloudPolicyManager* policy_manager,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory)
    : UserPolicySigninServiceBase(local_state,
                                  device_management_service,
                                  policy_manager,
                                  identity_manager,
                                  system_url_loader_factory),
      browser_state_prefs_(browser_state_prefs) {
  TryInitialize();
}

UserPolicySigninService::~UserPolicySigninService() {}

void UserPolicySigninService::Shutdown() {
  scoped_identity_manager_observation_.Reset();
  CancelPendingRegistration();
  UserPolicySigninServiceBase::Shutdown();
}

void UserPolicySigninService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  if (IsTurnOffSyncEvent(event)) {
    ShutdownUserCloudPolicyManager();
  }
}

void UserPolicySigninService::TryInitialize() {
  // If using a TestingProfile with no IdentityManager or
  // UserCloudPolicyManager, skip initialization.
  if (!policy_manager() || !identity_manager()) {
    DVLOG(1) << "Skipping initialization for tests due to missing components.";
    return;
  }

  // Shutdown the UserCloudPolicyManager when the user signs out. We start
  // observing the IdentityManager here because we don't want to get signout
  // notifications until after the profile has started initializing
  // (http://crbug.com/316229).
  scoped_identity_manager_observation_.Observe(identity_manager());

  if (!IsUserPolicyEnabled() ||
      !CanApplyPolicies(/*check_for_refresh_token=*/false)) {
    // Clear existing user policies if the feature is disabled or if policies
    // can no longer be applied.
    ShutdownUserCloudPolicyManager();
    return;
  }
  AccountId account_id =
      AccountIdFromAccountInfo(identity_manager()->GetPrimaryAccountInfo(
          GetConsentLevelForRegistration()));
  InitializeForSignedInUser(account_id, system_url_loader_factory());
}

bool UserPolicySigninService::CanApplyPolicies(bool check_for_refresh_token) {
  return CanApplyPoliciesForSignedInUser(check_for_refresh_token,
                                         GetConsentLevelForRegistration(),
                                         identity_manager());
}

base::TimeDelta UserPolicySigninService::GetTryRegistrationDelay() {
  return GetTryRegistrationDelayFromPrefs(browser_state_prefs_);
}

void UserPolicySigninService::ProhibitSignoutIfNeeded() {}

void UserPolicySigninService::UpdateLastPolicyCheckTime() {
  UpdateLastPolicyCheckTimeInPrefs(browser_state_prefs_);
}

signin::ConsentLevel UserPolicySigninService::GetConsentLevelForRegistration() {
  return signin::ConsentLevel::kSync;
}

}  // namespace policy
