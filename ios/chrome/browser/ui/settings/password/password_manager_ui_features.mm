// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"

namespace password_manager::features {
// When enabled, biometric authentication (Face ID, Touch ID or Passcode) is
// required to view saved credentials in the Password Manager.
BASE_FEATURE(kIOSPasswordAuthOnEntry,
             "IOSPasswordAuthOnEntry",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAuthOnEntryEnabled() {
  return base::FeatureList::IsEnabled(kIOSPasswordAuthOnEntry);
}
}  // namespace password_manager::features
