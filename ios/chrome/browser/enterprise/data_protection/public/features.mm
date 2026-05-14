// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/public/features.h"

BASE_FEATURE(kEnableScreenshotProtectionIOS, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsEnableScreenshotProtectionIOSEnabled() {
  return base::FeatureList::IsEnabled(kEnableScreenshotProtectionIOS);
}
