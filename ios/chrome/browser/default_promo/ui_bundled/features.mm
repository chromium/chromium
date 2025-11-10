// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/features.h"

BASE_FEATURE(kDefaultBrowserPromoIpadInstructions,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDefaultBrowserPromoIpadInstructions() {
  return base::FeatureList::IsEnabled(kDefaultBrowserPromoIpadInstructions);
}
