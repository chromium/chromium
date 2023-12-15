// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POLICY_IDLE_IDLE_TIMEOUT_POLICY_UTILS_H_
#define IOS_CHROME_BROWSER_UI_POLICY_IDLE_IDLE_TIMEOUT_POLICY_UTILS_H_

#import "base/containers/flat_set.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace enterprise_idle {

struct ActionSet {
  bool clear;    // True if any of ActionType::kClear* is present.
  bool close;    // True if ActionType::kCloseTabs is present.
  bool signout;  // True if any of ActionType::kSignOut is present.
};

// Returns the action set based on the value of `IdleTimeoutActions`.
// The action set only contains `signout` if the user is signed in.
ActionSet GetActionSet(PrefService* prefs, AuthenticationService* auth_service);

// Returns the string id that should be used for the title of the confirmation
// dialog telling the user what actions are about to be performed.
std::optional<int> GetIdleTimeoutActionsTitleId(ActionSet actions);
// Returns the string id that should be used for the subtitle of the
// confirmation dialog. The subtitle has an additional sentence if data will be
// cleared.
int GetIdleTimeoutActionsSubtitleId(ActionSet actions);
// Returns the string id for the message that will be shown in the snackbar
// after the idle timeout actions have run.
std::optional<int> GetIdleTimeoutActionsSnackbarMessageId(ActionSet actions);

}  // namespace enterprise_idle

#endif  // IOS_CHROME_BROWSER_UI_POLICY_IDLE_IDLE_TIMEOUT_POLICY_UTILS_H_
