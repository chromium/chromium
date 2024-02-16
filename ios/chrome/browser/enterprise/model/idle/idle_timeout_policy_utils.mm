// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/model/idle/idle_timeout_policy_utils.h"

#import "components/enterprise/idle/action_type.h"
#import "components/prefs/pref_service.h"

namespace enterprise_idle {

ActionSet GetActionSet(PrefService* prefs,
                       AuthenticationService* auth_service) {
  std::vector<ActionType> action_types = GetActionTypesFromPrefs(prefs);
  ActionSet action_set;
  for (ActionType action_type : action_types) {
    switch (action_type) {
      case ActionType::kCloseTabs:
        action_set.close = true;
        break;

        // The dialog and snackbar should not say "signed out" if the user was
        // not signed in before actions run.
      case ActionType::kSignOut:
        action_set.signout =
            auth_service->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
        break;

      case ActionType::kClearBrowsingHistory:
      case ActionType::kClearCookiesAndOtherSiteData:
      case ActionType::kClearCachedImagesAndFiles:
      case ActionType::kClearPasswordSignin:
      case ActionType::kClearAutofill:
        action_set.clear = true;
        break;
    }
  }
  return action_set;
}

}  // namespace enterprise_idle
