// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_util.h"

#import "base/check_deref.h"
#import "base/command_line.h"
#import "components/regional_capabilities/regional_capabilities_metrics.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engines_switches.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector.h"
#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_triggering_service.h"
#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_triggering_service_factory.h"
#import "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

namespace {
// Whether the choice screen might be displayed. The choice screen is by default
// disabled for tests or for non-branded builds. This method eliminates those
// cases, unless it is force-enabled by flag.
bool IsChoiceEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceSearchEngineChoiceScreen)) {
    return true;
  }
  if (tests_hook::DisableDefaultSearchEngineChoice()) {
    // This branch is taken in every other test.
    return false;
  }
  if (ios::provider::DisableDefaultSearchEngineChoice()) {
    // Outside of tests, this view should be disabled upstream.
    return false;
  }
  return true;
}
}  // namespace

bool ShouldDisplaySearchEngineChoiceScreen(
    ProfileIOS& profile,
    bool is_first_run_entrypoint,
    bool app_started_via_external_intent) {
  if (!IsChoiceEnabled()) {
    // This build is not eligible for the choice screen.
    return false;
  }
  ProfileIOS* original_profile = profile.GetOriginalProfile();
  search_engines::SearchEngineChoiceService* search_engine_choice_service =
      ios::SearchEngineChoiceServiceFactory::GetForProfile(original_profile);
  ios::SearchEngineChoiceTriggeringService* triggering_service =
      ios::SearchEngineChoiceTriggeringServiceFactory::GetForProfile(&profile);

  search_engines::SearchEngineChoiceScreenConditions condition;
  if (triggering_service) {
    condition = triggering_service->EvaluateTriggeringConditions(
        is_first_run_entrypoint, app_started_via_external_intent);
    search_engine_choice_service->RecordTriggeringEligibility(condition);
  } else {
    // TODO(crbug.com/438717568): This branch is added only to record the legacy
    // histograms. Investigate whether we need to keep it, or if we're fine with
    // updating the record timing of these old histogram.
    const policy::PolicyService& policy_service =
        *original_profile->GetPolicyConnector()->GetPolicyService();
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForProfile(original_profile);
    condition = search_engine_choice_service->GetStaticChoiceScreenConditions(
        policy_service, CHECK_DEREF(template_url_service));
    if (condition ==
        search_engines::SearchEngineChoiceScreenConditions::kEligible) {
      // If we didn't get a `triggering_service`, the search engine should not
      // be eligible for choice screens either.
      condition = search_engines::SearchEngineChoiceScreenConditions::
          kUnsupportedBrowserType;
    }
  }

  // This is today recording a combination of the static & dynamic
  // eligibilities.
  // TODO(crbug.com/426533078): Split this.
  search_engine_choice_service->RecordLegacyStaticEligibility(condition);
  return condition ==
         search_engines::SearchEngineChoiceScreenConditions::kEligible;
}
