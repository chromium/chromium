// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_util.h"

#import "base/check_deref.h"
#import "base/command_line.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "components/search_engines/search_engines_pref_names.h"
#import "components/search_engines/search_engines_switches.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/policy/model/browser_state_policy_connector.h"
#import "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_ui_util.h"
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
  // Getting data needed to check condition.
  search_engines::SearchEngineChoiceService* search_engine_choice_service =
      ios::SearchEngineChoiceServiceFactory::GetForProfile(original_profile);
  BrowserStatePolicyConnector* policy_connector =
      original_profile->GetPolicyConnector();
  const policy::PolicyService& policy_service =
      *policy_connector->GetPolicyService();
  TemplateURLService* template_url_service =
      ios::TemplateURLServiceFactory::GetForProfile(original_profile);

  // Checking whether the user is eligible for the screen.
  auto condition =
      search_engine_choice_service->GetStaticChoiceScreenConditions(
          policy_service, /*is_regular_profile=*/true,
          CHECK_DEREF(template_url_service));
  if (condition ==
      search_engines::SearchEngineChoiceScreenConditions::kEligible) {
    condition = search_engine_choice_service->GetDynamicChoiceScreenConditions(
        *template_url_service);
  }

  // If the app has been started via an external intent, and skip the Dialog
  // promo up to switches::kSearchEngineChoiceMaximumSkipCount() times.
  if (app_started_via_external_intent && !is_first_run_entrypoint &&
      condition ==
          search_engines::SearchEngineChoiceScreenConditions::kEligible) {
    PrefService* pref_service = original_profile->GetPrefs();
    const int count = pref_service->GetInteger(
        prefs::kDefaultSearchProviderChoiceScreenSkippedCount);

    if (count < switches::kSearchEngineChoiceMaximumSkipCount.Get()) {
      pref_service->SetInteger(
          prefs::kDefaultSearchProviderChoiceScreenSkippedCount, count + 1);

      condition = search_engines::SearchEngineChoiceScreenConditions::
          kAppStartedByExternalIntent;
    }
  }

  RecordChoiceScreenProfileInitCondition(condition);
  return condition ==
         search_engines::SearchEngineChoiceScreenConditions::kEligible;
}
