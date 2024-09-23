// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_POLICY_UTILS_H_
#define IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_POLICY_UTILS_H_

#import <optional>

#import "base/containers/flat_set.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_timeout_policy_utils.h"

namespace enterprise_idle {

// Returns the string id that should be used for the title of the confirmation
// dialog telling the user what actions are about to be performed.
std::optional<int> GetIdleTimeoutActionsTitleId(ActionSet actions);
// Returns the string id that should be used for the subtitle of the
// confirmation dialog. The subtitle has an additional sentence if data will be
// cleared or if the user will be signed out from their managed account in an
// unmanaged browser.
int GetIdleTimeoutActionsSubtitleId(ActionSet actions,
                                    bool is_data_cleared_on_signout);
// Returns the string id for the message that will be shown in the snackbar
// after the idle timeout actions have run.
std::optional<int> GetIdleTimeoutActionsSnackbarMessageId(ActionSet actions);

}  // namespace enterprise_idle

#endif  // IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_POLICY_UTILS_H_
