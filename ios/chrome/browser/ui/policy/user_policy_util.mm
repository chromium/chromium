// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/policy/user_policy_util.h"

#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/cloud/user_policy_switch.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool IsUserPolicyNotificationNeeded(AuthenticationService* authService,
                                    PrefService* prefService) {
  if (!policy::IsUserPolicyEnabled()) {
    return false;
  }

  if (prefService->GetBoolean(
          policy::policy_prefs::kUserPolicyNotificationWasShown)) {
    // Return false the notification was already shown in the past.
    return false;
  }

  // Return true if the primary identity is managed. Return false if there is
  // no account that is syncing (can be signed out or signed in with sync off).
  return authService->HasPrimaryIdentityManaged(signin::ConsentLevel::kSync);
}
