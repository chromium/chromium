// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_METRICS_UTILS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_METRICS_UTILS_H_

#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_details/credential_details.h"

namespace password_manager {

// Maps a DetailsContext to a WarningType for metrics
// logging.
WarningType GetWarningTypeForDetailsContext(DetailsContext details_context);

// Whether Password Check User Actions in Password Details should be recorded.
// `details_context` indicates if Password Details was opened from the Password
// Manager, Password Check UI or other entry points. `is_password_compromised`
// indicates if the credential upon which the user is acting is being displayed
// as compromised. Actions should only be recorded for credentials displayed as
// compromised and for credentials displayed in Password Details in one of the
// contexts related to the Password Check UI.
bool ShouldRecordPasswordCheckUserAction(DetailsContext details_context,
                                         bool is_password_compromised);

}  // namespace password_manager

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_METRICS_UTILS_H_
