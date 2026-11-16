// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/promo/public/features.h"

#import "base/check.h"
#import "base/metrics/field_trial_params.h"

BASE_FEATURE(kDefaultBrowserPromoIpadInstructions,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDefaultBrowserNonModalPromoStrings,
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kDefaultBrowserNonModalPromoStringsParam[] = "variant";

BASE_FEATURE_PARAM(
    int,
    kDefaultBrowserNonModalPromoStringsFeatureParam,
    &kDefaultBrowserNonModalPromoStrings,
    kDefaultBrowserNonModalPromoStringsParam,
    static_cast<int>(DefaultBrowserNonModalPromoStringsArm::kSkipCopyPaste));

bool IsDefaultBrowserPromoIpadInstructions() {
  return base::FeatureList::IsEnabled(kDefaultBrowserPromoIpadInstructions);
}

bool IsDefaultBrowserNonModalPromoStringsEnabled() {
  return base::FeatureList::IsEnabled(kDefaultBrowserNonModalPromoStrings);
}

DefaultBrowserNonModalPromoStringsArm
GetDefaultBrowserNonModalPromoStringsArm() {
  CHECK(IsDefaultBrowserNonModalPromoStringsEnabled());
  const int value = kDefaultBrowserNonModalPromoStringsFeatureParam.Get();
  switch (value) {
    case static_cast<int>(
        DefaultBrowserNonModalPromoStringsArm::kSkipCopyPaste):
      return DefaultBrowserNonModalPromoStringsArm::kSkipCopyPaste;
    case static_cast<int>(DefaultBrowserNonModalPromoStringsArm::kFewerSteps):
      return DefaultBrowserNonModalPromoStringsArm::kFewerSteps;
  }
  return DefaultBrowserNonModalPromoStringsArm::kSkipCopyPaste;
}
