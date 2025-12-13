// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_triggering_service.h"

#import "components/policy/core/common/policy_service.h"
#import "components/prefs/pref_service.h"
#import "components/regional_capabilities/regional_capabilities_metrics.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engines_pref_names.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_util.h"
#import "ios/chrome/browser/search_engine_choice/ui/search_engine_choice_ui_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

SearchEngineChoiceTriggeringService::SearchEngineChoiceTriggeringService(
    PrefService& profile_prefs,
    const policy::PolicyService& policy_service,
    search_engines::SearchEngineChoiceService& search_engine_choice_service,
    const TemplateURLService& template_url_service)
    : profile_prefs_(profile_prefs),
      policy_service_(policy_service),
      search_engine_choice_service_(search_engine_choice_service),
      template_url_service_(template_url_service) {}

SearchEngineChoiceTriggeringService::~SearchEngineChoiceTriggeringService() =
    default;

search_engines::SearchEngineChoiceScreenConditions
SearchEngineChoiceTriggeringService::EvaluateTriggeringConditions(
    bool is_first_run_entrypoint,
    bool app_started_via_external_intent) {
  if (!search_engine_choice_service_->IsSurfaceEligible(
          is_first_run_entrypoint)) {
    return search_engines::SearchEngineChoiceScreenConditions::
        kIneligibleSurface;
  }

  if (auto conditions =
          search_engine_choice_service_->GetStaticChoiceScreenConditions(
              policy_service_.get(), template_url_service_.get());
      conditions !=
      search_engines::SearchEngineChoiceScreenConditions::kEligible) {
    return conditions;
  }

  if (auto conditions =
          search_engine_choice_service_->GetDynamicChoiceScreenConditions(
              template_url_service_.get());
      conditions !=
      search_engines::SearchEngineChoiceScreenConditions::kEligible) {
    return conditions;
  }

  // If the app has been started via an external intent, skip the Dialog
  // promo up to `kSearchEngineChoiceMaximumSkipCount` times.
  if (app_started_via_external_intent && !is_first_run_entrypoint) {
    const int count = profile_prefs_->GetInteger(
        prefs::kDefaultSearchProviderChoiceScreenSkippedCount);

    if (count < kSearchEngineChoiceMaximumSkipCount) {
      profile_prefs_->SetInteger(
          prefs::kDefaultSearchProviderChoiceScreenSkippedCount, count + 1);

      return search_engines::SearchEngineChoiceScreenConditions::
          kAppStartedByExternalIntent;
    }
  }

  return search_engines::SearchEngineChoiceScreenConditions::kEligible;
}

}  // namespace ios
