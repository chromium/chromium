// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/features.h"

BASE_FEATURE(kTailoredNonModalDBPromo,
             "TailoredNonModalDBPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTailoredNonModalDBPromoEnabled() {
  return base::FeatureList::IsEnabled(kTailoredNonModalDBPromo);
}

BASE_FEATURE(kShareDefaultBrowserStatus,
             "ShareDefaultBrowserStatus",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsShareDefaultBrowserStatusEnabled() {
  return base::FeatureList::IsEnabled(kShareDefaultBrowserStatus);
}
