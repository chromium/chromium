// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/google_services/features.h"

BASE_FEATURE(kLinkedServicesSettingIos, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsLinkedServicesSettingIosEnabled() {
  return base::FeatureList::IsEnabled(kLinkedServicesSettingIos);
}
