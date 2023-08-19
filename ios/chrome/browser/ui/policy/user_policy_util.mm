// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/policy/user_policy_util.h"

#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/policy/cloud/user_policy_switch.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"

bool IsUserPolicyNotificationNeeded(AuthenticationService* authService,
                                    PrefService* prefService) {
  if (!policy::IsAnyUserPolicyFeatureEnabled()) {
    // Return false immediately if none of the user policy features is enabled.
    return false;
  }

  if (prefService->GetBoolean(
          policy::policy_prefs::kUserPolicyNotificationWasShown)) {
    // Return false the notification was already shown in the past.
    return false;
  }

  // TODO(crbug.com/1462552): Remove kSync usage after users are migrated to
  // kSignin only after kSync sunset. See ConsentLevel::kSync for more details.

  bool enabled_for_signin =
      policy::IsUserPolicyEnabledForSigninAndNoSyncConsentLevel();
  bool enabled_for_sync =
      policy::IsUserPolicyEnabledForSigninOrSyncConsentLevel();

  if (!enabled_for_sync &&
      authService->HasPrimaryIdentityManaged(signin::ConsentLevel::kSync)) {
    // Return false if sync is turned ON while the feature for that consent
    // level isn't enabled.
    return false;
  }

  // Set the minimal consent level to kSignin if User Policy is enabled for
  // signed in users, or otherwise to kSync which is the only other option.
  signin::ConsentLevel consent_level = enabled_for_signin
                                           ? signin::ConsentLevel::kSignin
                                           : signin::ConsentLevel::kSync;

  // Return true if the primary identity is managed with the minimal
  // `consent_level` to enable User Policy.
  return authService->HasPrimaryIdentityManaged(consent_level);
}
