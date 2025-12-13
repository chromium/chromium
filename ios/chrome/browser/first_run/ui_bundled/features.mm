// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/features.h"

#import "base/metrics/field_trial_params.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace first_run {

BASE_FEATURE(kAnimatedDefaultBrowserPromoInFRE,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBestFeaturesScreenInFirstRun,
             "BestFeaturesScreenInFirstRunExperience",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kManualLogUploadsInTheFRE, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSkipDefaultBrowserPromoInFirstRun,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUpdatedFirstRunSequence, base::FEATURE_DISABLED_BY_DEFAULT);

const char kAnimatedDefaultBrowserPromoInFREExperimentType[] =
    "AnimatedDefaultBrowserPromoInFREExperimentType";

const char kBestFeaturesScreenInFirstRunParam[] =
    "BestFeaturesScreenInFirstRunParam";

const char kUpdatedFirstRunSequenceParam[] = "updated-first-run-sequence-param";

BestFeaturesScreenVariationType GetBestFeaturesScreenVariationType() {
  if (!base::FeatureList::IsEnabled(kBestFeaturesScreenInFirstRun)) {
    return BestFeaturesScreenVariationType::kDisabled;
  }
  return static_cast<BestFeaturesScreenVariationType>(
      base::GetFieldTrialParamByFeatureAsInt(kBestFeaturesScreenInFirstRun,
                                             kBestFeaturesScreenInFirstRunParam,
                                             1));
}

UpdatedFRESequenceVariationType GetUpdatedFRESequenceVariation(
    ProfileIOS* profile) {
  regional_capabilities::RegionalCapabilitiesService* regional_capabilities =
      ios::RegionalCapabilitiesServiceFactory::GetForProfile(profile);
  if (!base::FeatureList::IsEnabled(kUpdatedFirstRunSequence) ||
      regional_capabilities->IsInSearchEngineChoiceScreenRegion()) {
    return UpdatedFRESequenceVariationType::kDisabled;
  }
  return static_cast<UpdatedFRESequenceVariationType>(
      base::GetFieldTrialParamByFeatureAsInt(kUpdatedFirstRunSequence,
                                             kUpdatedFirstRunSequenceParam, 1));
}

bool IsAnimatedDefaultBrowserPromoInFREEnabled() {
  return base::FeatureList::IsEnabled(kAnimatedDefaultBrowserPromoInFRE) &&
         !base::FeatureList::IsEnabled(first_run::kUpdatedFirstRunSequence);
}

AnimatedDefaultBrowserPromoInFREExperimentType
AnimatedDefaultBrowserPromoInFREExperimentTypeEnabled() {
  return static_cast<AnimatedDefaultBrowserPromoInFREExperimentType>(
      base::GetFieldTrialParamByFeatureAsInt(
          kAnimatedDefaultBrowserPromoInFRE,
          kAnimatedDefaultBrowserPromoInFREExperimentType, /*default_value=*/
          static_cast<int>(AnimatedDefaultBrowserPromoInFREExperimentType::
                               kAnimationWithActionButtons)));
}

}  // namespace first_run
