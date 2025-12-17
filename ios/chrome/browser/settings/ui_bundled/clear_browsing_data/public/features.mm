// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/public/features.h"

BASE_FEATURE(kPasswordRemovalFromDeleteBrowsingData,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPasswordRemovalFromDeleteBrowsingDataEnabled() {
  return base::FeatureList::IsEnabled(kPasswordRemovalFromDeleteBrowsingData);
}
