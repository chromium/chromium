// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_ui_features.h"

#import "base/feature_list.h"

namespace password_manager::features {

BASE_FEATURE(kIOSEnablePasscodeSettings, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordCheckupUIDoubleStartMitigation,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsPasscodeSettingsEnabled() {
  return base::FeatureList::IsEnabled(kIOSEnablePasscodeSettings);
}

}  // namespace password_manager::features
