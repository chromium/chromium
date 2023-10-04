// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"

namespace password_manager::features {
// When enabled, local authentication (Face ID, Touch ID or Passcode) is
// required to view saved credentials in the Password Manager Main Page.
BASE_FEATURE(kIOSPasswordAuthOnEntry,
             "IOSPasswordAuthOnEntry",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsAuthOnEntryEnabled() {
  return base::FeatureList::IsEnabled(kIOSPasswordAuthOnEntry);
}

// When enabled, local authentication (Face ID, Touch ID or Passcode) is
// required to view saved credentials in all Password Manager Surfaces.
BASE_FEATURE(kIOSPasswordAuthOnEntryV2,
             "IOSPasswordAuthOnEntryV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAuthOnEntryV2Enabled() {
  return base::FeatureList::IsEnabled(kIOSPasswordAuthOnEntryV2);
}

}  // namespace password_manager::features
