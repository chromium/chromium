// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/ui_bundled/user_policy_util.h"

#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/policy/core/common/policy_bundle.h"
#import "components/policy/core/common/policy_map.h"
#import "components/policy/core/common/policy_namespace.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"

namespace {

// Returns yes if the browser has machine level policies.
bool HasMachineLevelPolicies() {
  BrowserPolicyConnectorIOS* policy_connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  return policy_connector && policy_connector->HasMachineLevelPolicies();
}

// Returns true if the `provider` has at least one policy.
bool HasAtLeastOnePolicy(
    const policy::UserCloudPolicyManager* user_policy_manager) {
  const policy::PolicyMap& policy_map = user_policy_manager->policies().Get(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
  return !policy_map.empty();
}

}  // namespace

bool IsUserPolicyNotificationNeeded(
    AuthenticationService* authService,
    PrefService* prefService,
    const policy::UserCloudPolicyManager* user_policy_manager) {
  if (prefService->GetBoolean(
          policy::policy_prefs::kUserPolicyNotificationWasShown)) {
    // Return false the notification was already shown in the past.
    return false;
  }

  if (AreSeparateProfilesForManagedAccountsEnabled()) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(
          policy::kShowUserPolicyNotificationAtStartupIfNeeded)) {
    return false;
  }

  if (HasMachineLevelPolicies()) {
    // Return false if the browser is already managed at the machine level where
    // the user already knows that their browser is managed.
    return false;
  }

  if (!user_policy_manager || !HasAtLeastOnePolicy(user_policy_manager)) {
    // Return false if can't be determined that there is at least one user
    // policy.
    return false;
  }

  return CanFetchUserPolicy(authService, prefService);
}

bool CanFetchUserPolicy(AuthenticationService* authService,
                        PrefService* prefService) {
  // Return true if the primary identity is managed.
  return authService->HasPrimaryIdentityManaged(signin::ConsentLevel::kSignin);
}
