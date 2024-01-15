// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_util.h"

#import "components/search_engines/search_engine_choice_utils.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/policy/model/browser_state_policy_connector.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

bool ShouldDisplaySearchEngineChoiceScreen(SceneState* scene_state) {
  if (!IsChoiceEnabled()) {
    return false;
  }
  ChromeBrowserState* browser_state =
      scene_state.browserProviderInterface.mainBrowserProvider.browser
          ->GetBrowserState();
  if (!browser_state) {
    return false;
  }
  BrowserStatePolicyConnector* policy_connector =
      browser_state->GetPolicyConnector();
  return search_engines::ShouldShowChoiceScreen(
      *policy_connector->GetPolicyService(),
      /*profile_properties=*/
      {.is_regular_profile = true, .pref_service = browser_state->GetPrefs()},
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state));
}

bool IsChoiceEnabled() {
  if (experimental_flags::AlwaysDisplaySearchEngineChoice()) {
    // This branch is only selected in tests that are related to choice screen.
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
  return search_engines::IsChoiceScreenFlagEnabled(
      search_engines::ChoicePromo::kDialog);
}
