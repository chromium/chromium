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
  if (prefService->GetBoolean(
          policy::policy_prefs::kUserPolicyNotificationWasShown)) {
    // Return false the notification was already shown in the past.
    return false;
  }

  return CanFetchUserPolicy(authService, prefService);
}

bool CanFetchUserPolicy(AuthenticationService* authService,
                        PrefService* prefService) {
  if (!policy::IsAnyUserPolicyFeatureEnabled()) {
    // Return false immediately if the user policy features isn't enabled for
    // the minimal consent level.
    return false;
  }

  // TODO(crbug.com/1462552): Remove kSync usage after users are migrated to
  // kSignin only after kSync sunset. See ConsentLevel::kSync for more details.

  bool enabled_for_sync =
      policy::IsUserPolicyEnabledForSigninOrSyncConsentLevel();

  if (!enabled_for_sync &&
      authService->HasPrimaryIdentityManaged(signin::ConsentLevel::kSync)) {
    // Return false if sync is turned ON while the feature for that consent
    // level isn't enabled.
    return false;
  }

  // Return true if the primary identity is managed with the minimal
  // `consent_level` to enable User Policy.
  return authService->HasPrimaryIdentityManaged(signin::ConsentLevel::kSignin);
}
