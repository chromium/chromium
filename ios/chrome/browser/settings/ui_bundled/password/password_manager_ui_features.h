// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_MANAGER_UI_FEATURES_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_MANAGER_UI_FEATURES_H_

#import "base/feature_list.h"

// Password manager features that only touch UI. Other features should go in
// components.
namespace password_manager::features {

// Kill switch for the logic that allows the user to open the native Password
// Settings page. Used when the user wants to access the Password Manager UI
// without a passcode set.
BASE_DECLARE_FEATURE(kIOSEnablePasscodeSettings);

// Enable a fix to mitigate an issue where child coordinators on the password
// checkup UI (e.g. the password issues UI) are double started. Enabled by
// default, act as a kill switch.
BASE_DECLARE_FEATURE(kPasswordCheckupUIDoubleStartMitigation);

// Helper function returning the status of `kIOSEnablePasscodeSettings`.
bool IsPasscodeSettingsEnabled();

}  // namespace password_manager::features

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_MANAGER_UI_FEATURES_H_
