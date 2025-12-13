// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/privacy/privacy_guide/features.h"

BASE_FEATURE(kPrivacyGuideIos, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPrivacyGuideIosEnabled() {
  return base::FeatureList::IsEnabled(kPrivacyGuideIos);
}
