// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/features.h"

#import "base/feature_list.h"

BASE_FEATURE(kFullscreenPromosManagerSkipInternalLimits,
             "FullscreenPromosManagerSkipInternalLimits",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPromosManagerUsesFET,
             "PromosManagerUsesFET",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsSkippingInternalImpressionLimitsEnabled() {
  return base::FeatureList::IsEnabled(
      kFullscreenPromosManagerSkipInternalLimits);
}

bool ShouldPromosManagerUseFET() {
  return base::FeatureList::IsEnabled(kPromosManagerUsesFET);
}
