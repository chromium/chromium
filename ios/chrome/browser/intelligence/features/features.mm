// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/features/features.h"

#import "base/check.h"
#import "base/metrics/field_trial_params.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/public/features/features.h"

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
  if (IsDiamondPrototypeEnabled()) {
    return true;
  }
  return base::FeatureList::IsEnabled(kPageActionMenu);
}

BASE_FEATURE(kGeminiCrossTab,
             "GeminiCrossTab",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGeminiCrossTabEnabled() {
  if (!IsPageActionMenuEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kGeminiCrossTab);
}

bool IsDirectBWGEntryPoint() {
  CHECK(IsPageActionMenuEnabled());
  return base::GetFieldTrialParamByFeatureAsBool(
      kPageActionMenu, kPageActionMenuDirectEntryPointParam, false);
}

const char kBWGSessionValidityDurationParam[] = "BWGSessionValidityDuration";

const base::TimeDelta BWGSessionValidityDuration() {
  return base::Minutes(base::GetFieldTrialParamByFeatureAsInt(
      kPageActionMenu, kBWGSessionValidityDurationParam, 30));
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
    return BWGPromoConsentVariations::kForceFRE;
  }
  return BWGPromoConsentVariations::kDisabled;
}

bool ShouldForceBWGPromo() {
  return BWGPromoConsentVariationsParam() ==
         BWGPromoConsentVariations::kForceFRE;
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

BASE_FEATURE(kBWGPreciseLocation,
             "BWGPreciseLocation",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsBWGPreciseLocationEnabled() {
  CHECK(IsPageActionMenuEnabled());
  return base::FeatureList::IsEnabled(kBWGPreciseLocation);
}

BASE_FEATURE(kPageContextAnchorTags,
             "PageContextAnchorTags",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPageContextAnchorTagsEnabled() {
  return base::FeatureList::IsEnabled(kPageContextAnchorTags);
}
