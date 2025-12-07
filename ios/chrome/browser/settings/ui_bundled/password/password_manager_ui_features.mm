// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_ui_features.h"

namespace password_manager::features {

BASE_FEATURE(kIOSEnablePasscodeSettings, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSuggestStrongPasswordInAddPassword,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsPasscodeSettingsEnabled() {
  return base::FeatureList::IsEnabled(kIOSEnablePasscodeSettings);
}

bool IsSuggestStrongPasswordInAddPasswordEnabled() {
  return base::FeatureList::IsEnabled(kSuggestStrongPasswordInAddPassword);
}

}  // namespace password_manager::features
