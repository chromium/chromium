// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/policy/idle/idle_timeout_policy_utils.h"

#import "components/enterprise/idle/action_type.h"
#import "components/enterprise/idle/idle_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"

namespace enterprise_idle {

ActionSet GetActionSet(PrefService* prefs,
                       AuthenticationService* auth_service) {
  std::vector<ActionType> action_types = GetActionTypesFromPrefs(prefs);
  ActionSet action_set = {.clear = false, .close = false, .signout = false};
  for (ActionType action_type : action_types) {
    switch (action_type) {
      case ActionType::kCloseTabs:
        action_set.close = true;
        break;

      // The dialog and snackbar should not say "signed out" if the user was not
      // signed in before actions run.
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

      // TODO(b/301676922): Remove this once the ActionType enum has been
      // cleaned up
      default:
        break;
    }
  }
  return action_set;
}

absl::optional<int> GetIdleTimeoutActionsTitleId(ActionSet actions) {
  if (actions.clear && actions.close && actions.signout) {
    return IDS_IOS_IDLE_TIMEOUT_ALL_ACTIONS_TITLE;
  }
  if (actions.clear && actions.close) {
    return IDS_IOS_IDLE_TIMEOUT_CLOSE_TABS_AND_CLEAR_DATA_TITLE;
  }
  if (actions.clear && actions.signout) {
    return IDS_IOS_IDLE_TIMEOUT_CLEAR_DATA_AND_SIGNOUT_TITLE;
  }
  if (actions.close && actions.signout) {
    return IDS_IOS_IDLE_TIMEOUT_CLOSE_TABS_AND_SIGNOUT_TITLE;
  }
  if (actions.clear) {
    return IDS_IOS_IDLE_TIMEOUT_CLEAR_DATA_TITLE;
  }
  if (actions.signout) {
    return IDS_IOS_IDLE_TIMEOUT_SIGNOUT_TITLE;
  }
  if (actions.close) {
    return IDS_IOS_IDLE_TIMEOUT_CLOSE_TABS_TITLE;
  }
  // All cases should be covered by one of the title strings.
  return std::nullopt;
}

int GetIdleTimeoutActionsSubtitleId(ActionSet actions) {
  return actions.clear ? IDS_IOS_IDLE_TIMEOUT_SUBTITLE_WITH_CLEAR_DATA
                       : IDS_IOS_IDLE_TIMEOUT_SUBTITLE_WITHOUT_CLEAR_DATA;
}

absl::optional<int> GetIdleTimeoutActionsSnackbarMessageId(ActionSet actions) {
  if (actions.clear && actions.close && actions.signout) {
    return IDS_IOS_IDLE_TIMEOUT_ALL_ACTIONS_SNACKBAR_MESSAGE;
  }
  if (actions.clear && actions.close) {
    return IDS_IOS_IDLE_TIMEOUT_CLOSE_TABS_AND_CLEAR_DATA_SNACKBAR_MESSAGE;
  }
  if (actions.clear && actions.signout) {
    return IDS_IOS_IDLE_TIMEOUT_CLEAR_DATA_AND_SIGNOUT_SNACKBAR_MESSAGE;
  }
  if (actions.close && actions.signout) {
    return IDS_IOS_IDLE_TIMEOUT_CLOSE_TABS_AND_SIGNOUT_SNACKBAR_MESSAGE;
  }
  if (actions.clear) {
    return IDS_IOS_IDLE_TIMEOUT_CLEAR_DATA_SNACKBAR_MESSAGE;
  }
  if (actions.signout) {
    return IDS_IOS_IDLE_TIMEOUT_SIGNOUT_SNACKBAR_MESSAGE;
  }
  if (actions.close) {
    return IDS_IOS_IDLE_TIMEOUT_CLOSE_TABS_SNACKBAR_MESSAGE;
  }
  // All cases should be covered by one of the title strings.
  return std::nullopt;
}

}  // namespace enterprise_idle
