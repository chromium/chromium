// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/features.h"

#import "base/metrics/field_trial_params.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace first_run {

BASE_FEATURE(kUpdatedFirstRunSequence,
             "UpdatedFirstRunSequence",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kUpdatedFirstRunSequenceParam[] = "updated-first-run-sequence-param";

UpdatedFRESequenceVariationType GetUpdatedFRESequenceVariation(
    ProfileIOS* profile) {
  BOOL excluded_country = search_engines::IsEeaChoiceCountry(
      ios::SearchEngineChoiceServiceFactory::GetForProfile(profile)
          ->GetCountryId());

  if (!base::FeatureList::IsEnabled(kUpdatedFirstRunSequence) ||
      excluded_country) {
    return UpdatedFRESequenceVariationType::kDisabled;
  }
  return static_cast<UpdatedFRESequenceVariationType>(
      base::GetFieldTrialParamByFeatureAsInt(kUpdatedFirstRunSequence,
                                             kUpdatedFirstRunSequenceParam, 1));
}

BASE_FEATURE(kAnimatedDefaultBrowserPromoInFRE,
             "AnimatedDefaultBrowserPromoInFRE",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAnimatedDefaultBrowserPromoInFREEnabled() {
  return base::FeatureList::IsEnabled(kAnimatedDefaultBrowserPromoInFRE) &&
         !base::FeatureList::IsEnabled(first_run::kUpdatedFirstRunSequence);
}

}  // namespace first_run
