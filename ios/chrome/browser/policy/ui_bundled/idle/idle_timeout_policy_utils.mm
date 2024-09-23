// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_policy_utils.h"

#import "components/enterprise/idle/action_type.h"
#import "components/enterprise/idle/idle_pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"

namespace enterprise_idle {

std::optional<int> GetIdleTimeoutActionsTitleId(ActionSet actions) {
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

int GetIdleTimeoutActionsSubtitleId(ActionSet actions,
                                    bool is_data_cleared_on_signout) {
  if (actions.clear) {
    return IDS_IOS_IDLE_TIMEOUT_SUBTITLE_WITH_CLEAR_DATA;
  } else if (actions.signout && is_data_cleared_on_signout) {
    return IDS_IOS_IDLE_TIMEOUT_SUBTITLE_WITH_CLEAR_DATA_ON_SIGNOUT;
  } else {
    return IDS_IOS_IDLE_TIMEOUT_SUBTITLE_WITHOUT_CLEAR_DATA;
  }
}

std::optional<int> GetIdleTimeoutActionsSnackbarMessageId(ActionSet actions) {
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
