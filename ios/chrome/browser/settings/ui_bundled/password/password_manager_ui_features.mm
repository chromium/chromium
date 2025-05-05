// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_ui_features.h"

namespace password_manager::features {

BASE_FEATURE(kIOSEnablePasscodeSettings,
             "IOSEnablePasscodeSettings",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSEnableDeleteAllSavedCredentials,
             "IOSEnableDeleteAllSavedCredentials",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSuggestStrongPasswordInAddPassword,
             "SuggestStrongPasswordInAddPassword",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSEnablePasswordManagerTrustedVaultWidget,
             "IOSEnablePasswordManagerTrustedVaultWidget",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPasscodeSettingsEnabled() {
  return base::FeatureList::IsEnabled(kIOSEnablePasscodeSettings);
}

bool IsSuggestStrongPasswordInAddPasswordEnabled() {
  return base::FeatureList::IsEnabled(kSuggestStrongPasswordInAddPassword);
}

bool IsPasswordManagerTrustedVaultWidgetEnabled() {
  return base::FeatureList::IsEnabled(
      kIOSEnablePasswordManagerTrustedVaultWidget);
}

}  // namespace password_manager::features
