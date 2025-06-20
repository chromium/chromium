// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/features/features.h"

#import "base/check.h"
#import "base/metrics/field_trial_params.h"

BASE_FEATURE(kEnhancedCalendar,
             "EnhancedCalendar",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsEnhancedCalendarEnabled() {
  return base::FeatureList::IsEnabled(kEnhancedCalendar);
}

BASE_FEATURE(kPageActionMenu,
             "PageActionMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kPageActionMenuDirectEntryPointParam[] =
    "PageActionMenuDirectEntryPoint";

bool IsPageActionMenuEnabled() {
  return base::FeatureList::IsEnabled(kPageActionMenu);
}

bool IsDirectBWGEntryPoint() {
  CHECK(IsPageActionMenuEnabled());
  return base::GetFieldTrialParamByFeatureAsBool(
      kPageActionMenu, kPageActionMenuDirectEntryPointParam, false);
}

const char kBWGPromoConsentParams[] = "BWGPromoConsentVariations";

BWGPromoConsentVariations BWGPromoConsentVariationsParam() {
  int param = base::GetFieldTrialParamByFeatureAsInt(kBWGPromoConsent,
                                                     kBWGPromoConsentParams, 0);
  if (!IsPageActionMenuEnabled()) {
    return BWGPromoConsentVariations::kDisabled;
  }
  if (param == 1) {
    return BWGPromoConsentVariations::kSinglePage;
  }
  if (param == 2) {
    return BWGPromoConsentVariations::kDoublePage;
  }
  if (param == 3) {
    return BWGPromoConsentVariations::kSkipConsent;
  }
  if (param == 4) {
    return BWGPromoConsentVariations::kForceConsent;
  }
  return BWGPromoConsentVariations::kDisabled;
}

BASE_FEATURE(kBWGPromoConsent,
             "BWGPromoConsent",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kExplainGeminiEditMenuParams[] = "PositionForExplainGeminiEditMenu";

PositionForExplainGeminiEditMenu ExplainGeminiEditMenuPosition() {
  int param = base::GetFieldTrialParamByFeatureAsInt(
      kExplainGeminiEditMenu, kExplainGeminiEditMenuParams, 0);
  if (param == 1) {
    return PositionForExplainGeminiEditMenu::kAfterEdit;
  }
  if (param == 2) {
    return PositionForExplainGeminiEditMenu::kAfterSearch;
  }
  return PositionForExplainGeminiEditMenu::kDisabled;
}

BASE_FEATURE(kExplainGeminiEditMenu,
             "ExplainGeminiEditMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);
