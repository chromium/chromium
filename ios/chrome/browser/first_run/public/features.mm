// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/public/features.h"

#import "base/check.h"
#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace first_run {

BASE_FEATURE(kAnimatedDefaultBrowserPromoInFRE,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBestFeaturesScreenInFirstRun,
             "BestFeaturesScreenInFirstRunExperience",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPostFREIphInProfileAgent, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidSwitcherDefaultBrowserPromoInFRE,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRecordSwitcherInfoDefaultBrowserMetrics,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFREAblation, base::FEATURE_DISABLED_BY_DEFAULT);

const char kAnimatedDefaultBrowserPromoInFREExperimentType[] =
    "AnimatedDefaultBrowserPromoInFREExperimentType";

const char kBestFeaturesScreenInFirstRunParam[] =
    "BestFeaturesScreenInFirstRunParam";

const char kAndroidSwitcherDefaultBrowserPromoInFREParam[] =
    "android-switcher-default-browser-promo-in-fre-param";

const char kAndroidSwitcherDefaultBrowserPromoInFREParamDisabled[] = "disabled";

const char kAndroidSwitcherDefaultBrowserPromoInFREParamSegmentationSignal[] =
    "segmentation-signal";

const char kAndroidSwitcherDefaultBrowserPromoInFREParamSwitcherInfoSignal[] =
    "switcher-info-signal";

const char kAndroidSwitcherDefaultBrowserPromoInFREArmParam[] =
    "android-switcher-default-browser-promo-in-fre-arm";

const char kAndroidSwitcherDefaultBrowserPromoInFREArmGoogleEcosystem[] =
    "google-ecosystem";

const char kAndroidSwitcherDefaultBrowserPromoInFREArmHybrid[] = "hybrid";

const char kFREAblationParam[] = "fre-ablation-param";

const char kFREAblationParamSkipHistorySync[] = "skip-history-sync";

const char kFREAblationParamSkipAllExceptSignInAndChoice[] =
    "skip-all-except-sign-in-and-choice";

BASE_FEATURE_PARAM(int,
                   kBestFeaturesScreenInFirstRunParamFeature,
                   &kBestFeaturesScreenInFirstRun,
                   kBestFeaturesScreenInFirstRunParam,
                   1);

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
    AndroidSwitcherDefaultBrowserPromoInFREVariationType::kSegmentationSignal,
    &kAndroidSwitcherDefaultBrowserPromoInFREOptions);

constexpr base::FeatureParam<AndroidSwitcherDefaultBrowserPromoInFREArm>::Option
    kAndroidSwitcherDefaultBrowserPromoInFREArmOptions[] = {
        {AndroidSwitcherDefaultBrowserPromoInFREArm::kGoogleEcosystem,
         kAndroidSwitcherDefaultBrowserPromoInFREArmGoogleEcosystem},
        {AndroidSwitcherDefaultBrowserPromoInFREArm::kHybrid,
         kAndroidSwitcherDefaultBrowserPromoInFREArmHybrid},
};

BASE_FEATURE_ENUM_PARAM(
    AndroidSwitcherDefaultBrowserPromoInFREArm,
    kAndroidSwitcherDefaultBrowserPromoInFREArmFeature,
    &kAndroidSwitcherDefaultBrowserPromoInFRE,
    kAndroidSwitcherDefaultBrowserPromoInFREArmParam,
    AndroidSwitcherDefaultBrowserPromoInFREArm::kGoogleEcosystem,
    &kAndroidSwitcherDefaultBrowserPromoInFREArmOptions);

constexpr base::FeatureParam<FREAblationVariationType>::Option
    kFREAblationOptions[] = {
        {FREAblationVariationType::kSkipHistorySync,
         kFREAblationParamSkipHistorySync},
        {FREAblationVariationType::kSkipAllExceptSignInAndChoice,
         kFREAblationParamSkipAllExceptSignInAndChoice},
};

BASE_FEATURE_ENUM_PARAM(FREAblationVariationType,
                        kFREAblationParamFeature,
                        &kFREAblation,
                        kFREAblationParam,
                        FREAblationVariationType::kSkipHistorySync,
                        &kFREAblationOptions);

BestFeaturesScreenVariationType GetBestFeaturesScreenVariationType() {
  if (IsBestOfAppBestFeaturesEnabled()) {
    return BestFeaturesScreenVariationType::kBestOfApp;
  }
  if (!base::FeatureList::IsEnabled(kBestFeaturesScreenInFirstRun)) {
    return BestFeaturesScreenVariationType::kDisabled;
  }
  int param = kBestFeaturesScreenInFirstRunParamFeature.Get();
  if (param == 4) {
    // 4 was `kShoppingUsersWithFallbackAfterDBPromo` (the Price Tracking
    // promo), which is now obsolete.
    return BestFeaturesScreenVariationType::kDisabled;
  }
  return static_cast<BestFeaturesScreenVariationType>(param);
}

bool IsAnimatedDefaultBrowserPromoInFREEnabled() {
  return base::FeatureList::IsEnabled(kAnimatedDefaultBrowserPromoInFRE);
}

AnimatedDefaultBrowserPromoInFREExperimentType
AnimatedDefaultBrowserPromoInFREExperimentTypeEnabled() {
  return static_cast<AnimatedDefaultBrowserPromoInFREExperimentType>(
      kAnimatedDefaultBrowserPromoInFREExperimentTypeFeature.Get());
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

AndroidSwitcherDefaultBrowserPromoInFREArm
GetAndroidSwitcherDefaultBrowserPromoInFREArm() {
  return kAndroidSwitcherDefaultBrowserPromoInFREArmFeature.Get();
}

bool IsFREAblationEnabled() {
  return base::FeatureList::IsEnabled(kFREAblation);
}

FREAblationVariationType GetFREAblationVariationType() {
  CHECK(IsFREAblationEnabled());
  return static_cast<FREAblationVariationType>(kFREAblationParamFeature.Get());
}

}  // namespace first_run
