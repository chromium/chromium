// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/public/features.h"

#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace first_run {

BASE_FEATURE(kAnimatedDefaultBrowserPromoInFRE,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBestFeaturesScreenInFirstRun,
             "BestFeaturesScreenInFirstRunExperience",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSkipDefaultBrowserPromoInFirstRun,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUpdatedFirstRunSequence, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPostFREIphInProfileAgent, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidSwitcherDefaultBrowserPromoInFRE,
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kAnimatedDefaultBrowserPromoInFREExperimentType[] =
    "AnimatedDefaultBrowserPromoInFREExperimentType";

const char kBestFeaturesScreenInFirstRunParam[] =
    "BestFeaturesScreenInFirstRunParam";

const char kUpdatedFirstRunSequenceParam[] = "updated-first-run-sequence-param";

const char kAndroidSwitcherDefaultBrowserPromoInFREParam[] =
    "android-switcher-default-browser-promo-in-fre-param";

const char kAndroidSwitcherDefaultBrowserPromoInFREParamDisabled[] = "disabled";

const char kAndroidSwitcherDefaultBrowserPromoInFREParamSegmentationSignal[] =
    "segmentation-signal";

const char kAndroidSwitcherDefaultBrowserPromoInFREParamSwitcherInfoSignal[] =
    "switcher-info-signal";

BASE_FEATURE_PARAM(int,
                   kBestFeaturesScreenInFirstRunParamFeature,
                   &kBestFeaturesScreenInFirstRun,
                   kBestFeaturesScreenInFirstRunParam,
                   1);

BASE_FEATURE_PARAM(int,
                   kUpdatedFirstRunSequenceParamFeature,
                   &kUpdatedFirstRunSequence,
                   kUpdatedFirstRunSequenceParam,
                   2);

BASE_FEATURE_PARAM(
    int,
    kAnimatedDefaultBrowserPromoInFREExperimentTypeFeature,
    &kAnimatedDefaultBrowserPromoInFRE,
    kAnimatedDefaultBrowserPromoInFREExperimentType,
    static_cast<int>(AnimatedDefaultBrowserPromoInFREExperimentType::
                         kAnimationWithActionButtons));

constexpr base::FeatureParam<
    AndroidSwitcherDefaultBrowserPromoInFREVariationType>::Option
    kAndroidSwitcherDefaultBrowserPromoInFREOptions[] = {
        {AndroidSwitcherDefaultBrowserPromoInFREVariationType::kDisabled,
         kAndroidSwitcherDefaultBrowserPromoInFREParamDisabled},
        {AndroidSwitcherDefaultBrowserPromoInFREVariationType::
             kSegmentationSignal,
         kAndroidSwitcherDefaultBrowserPromoInFREParamSegmentationSignal},
        {AndroidSwitcherDefaultBrowserPromoInFREVariationType::
             kSwitcherInfoSignal,
         kAndroidSwitcherDefaultBrowserPromoInFREParamSwitcherInfoSignal},
};

BASE_FEATURE_ENUM_PARAM(
    AndroidSwitcherDefaultBrowserPromoInFREVariationType,
    kAndroidSwitcherDefaultBrowserPromoInFREParamFeature,
    &kAndroidSwitcherDefaultBrowserPromoInFRE,
    kAndroidSwitcherDefaultBrowserPromoInFREParam,
    AndroidSwitcherDefaultBrowserPromoInFREVariationType::kDisabled,
    &kAndroidSwitcherDefaultBrowserPromoInFREOptions);

BestFeaturesScreenVariationType GetBestFeaturesScreenVariationType() {
  if (IsBestOfAppBestFeaturesEnabled()) {
    return BestFeaturesScreenVariationType::kBestOfApp;
  }
  if (!base::FeatureList::IsEnabled(kBestFeaturesScreenInFirstRun)) {
    return BestFeaturesScreenVariationType::kDisabled;
  }
  return static_cast<BestFeaturesScreenVariationType>(
      kBestFeaturesScreenInFirstRunParamFeature.Get());
}

UpdatedFRESequenceVariationType GetUpdatedFRESequenceVariation() {
  if (!base::FeatureList::IsEnabled(kUpdatedFirstRunSequence)) {
    return UpdatedFRESequenceVariationType::kDisabled;
  }
  return static_cast<UpdatedFRESequenceVariationType>(
      kUpdatedFirstRunSequenceParamFeature.Get());
}

bool IsAnimatedDefaultBrowserPromoInFREEnabled() {
  return base::FeatureList::IsEnabled(kAnimatedDefaultBrowserPromoInFRE) &&
         !base::FeatureList::IsEnabled(first_run::kUpdatedFirstRunSequence);
}

AnimatedDefaultBrowserPromoInFREExperimentType
AnimatedDefaultBrowserPromoInFREExperimentTypeEnabled() {
  return static_cast<AnimatedDefaultBrowserPromoInFREExperimentType>(
      kAnimatedDefaultBrowserPromoInFREExperimentTypeFeature.Get());
}

bool IsSkipDefaultBrowserPromoInFirstRunEnabled(bool is_in_eea_country) {
  return is_in_eea_country &&
         base::FeatureList::IsEnabled(kSkipDefaultBrowserPromoInFirstRun);
}

bool IsPostFREIphInProfileAgentEnabled() {
  return base::FeatureList::IsEnabled(kPostFREIphInProfileAgent);
}

AndroidSwitcherDefaultBrowserPromoInFREVariationType
GetAndroidSwitcherDefaultBrowserPromoInFREVariationType() {
  if (!base::FeatureList::IsEnabled(kAndroidSwitcherDefaultBrowserPromoInFRE)) {
    return AndroidSwitcherDefaultBrowserPromoInFREVariationType::kDisabled;
  }
  return kAndroidSwitcherDefaultBrowserPromoInFREParamFeature.Get();
}

}  // namespace first_run
