// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/cloud/user_policy_signin_service.h"

#import "base/logging.h"
#import "base/time/time.h"
#import "components/policy/core/browser/cloud/user_policy_signin_service_util.h"
#import "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#import "components/policy/core/common/policy_logger.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/primary_account_change_event.h"
#import "google_apis/gaia/core_account_id.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_switch.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// TODO(crbug.com/40831740): Move
// chrome/browser/signin/account_id_from_account_info.h to components/ to be
// able to reuse the helper here.
//
// Gets the AccountId from the provided `account_info`.
AccountId AccountIdFromAccountInfo(const CoreAccountInfo& account_info) {
  if (account_info.email.empty() || account_info.gaia.empty())
    return EmptyAccountId();

  return AccountId::FromUserEmailGaiaId(
      gaia::CanonicalizeEmail(account_info.email), account_info.gaia);
}

}  // namespace

namespace policy {

UserPolicySigninService::UserPolicySigninService(
    PrefService* pref_service,
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
      pref_service_(pref_service) {
  if (identity_manager) {
    scoped_identity_manager_observation_.Observe(identity_manager);
  }

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
  if (IsSignoutEvent(event)) {
    ShutdownCloudPolicyManager();
  }
}

void UserPolicySigninService::TryInitialize() {
  // If using a TestingProfile with no IdentityManager or
  // CloudPolicyManager, skip initialization.
  if (!policy_manager() || !identity_manager()) {
    DVLOG_POLICY(1, POLICY_AUTH)
        << "Skipping initialization for tests due to missing components.";
    return;
  }

  if (!IsAnyUserPolicyFeatureEnabled() ||
      !CanApplyPolicies(/*check_for_refresh_token=*/false)) {
    // Clear existing user policies if the feature is disabled or if policies
    // can no longer be applied.
    DVLOG_POLICY(3, POLICY_PROCESSING)
        << "Clearing existing user policies as the feature is disabled";
    ShutdownCloudPolicyManager();
    return;
  }
  AccountId account_id =
      AccountIdFromAccountInfo(identity_manager()->GetPrimaryAccountInfo(
          GetConsentLevelForRegistration()));
  InitializeForSignedInUser(account_id, system_url_loader_factory());
}

bool UserPolicySigninService::CanApplyPolicies(bool check_for_refresh_token) {
  // Can't apply policies for an account that is using Sync if the feature isn't
  // explicitly enabled.
  bool sync_on =
      check_for_refresh_token
          ? identity_manager()->HasPrimaryAccountWithRefreshToken(
                signin::ConsentLevel::kSync)
          : identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync);
  if (!IsUserPolicyEnabledForSigninOrSyncConsentLevel() && sync_on) {
    return false;
  }

  return CanApplyPoliciesForSignedInUser(check_for_refresh_token,
                                         GetConsentLevelForRegistration(),
                                         identity_manager());
}

std::string UserPolicySigninService::GetProfileId() {
  // Profile ID hasn't been implemented on iOS yet.
  return std::string();
}

base::TimeDelta UserPolicySigninService::GetTryRegistrationDelay() {
  return GetTryRegistrationDelayFromPrefs(pref_service_);
}

void UserPolicySigninService::ProhibitSignoutIfNeeded() {}

void UserPolicySigninService::UpdateLastPolicyCheckTime() {
  UpdateLastPolicyCheckTimeInPrefs(pref_service_);
}

signin::ConsentLevel UserPolicySigninService::GetConsentLevelForRegistration() {
  return signin::ConsentLevel::kSignin;
}

void UserPolicySigninService::OnUserPolicyNotificationSeen() {
  TryInitialize();
}

}  // namespace policy
