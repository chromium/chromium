// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"

namespace password_manager::features {

// Kill switch for the logic that allows the user to open the native Password
// Settings page. Used when the user wants to access the Password Manager UI
// without a passcode set.
BASE_FEATURE(kIOSEnablePasscodeSettings,
             "IOSEnablePasscodeSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Helper function returning the status of `kIOSEnablePasscodeSettings`.
bool IsPasscodeSettingsEnabled() {
  return base::FeatureList::IsEnabled(kIOSEnablePasscodeSettings);
}

}  // namespace password_manager::features
