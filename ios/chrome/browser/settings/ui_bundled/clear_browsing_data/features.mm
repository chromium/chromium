// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/features.h"

BASE_FEATURE(kIOSQuickDelete,
             "kIOSQuickDelete",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsIosQuickDeleteEnabled() {
  return base::FeatureList::IsEnabled(kIOSQuickDelete);
}
