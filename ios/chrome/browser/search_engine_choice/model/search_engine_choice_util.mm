// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_util.h"

#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engine_choice_utils.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/policy/model/browser_state_policy_connector.h"
#import "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_ui_util.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

namespace {
// Whether the choice screen might be displayed. The choice screen is by default
// disabled for tests or for non-branded builds. This method eliminates those
// cases.
bool IsChoiceEnabled() {
  if (IsSearchEngineForceEnabled()) {
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
}  // namespace

bool ShouldDisplaySearchEngineChoiceScreen(Browser* browser) {
  if (!IsChoiceEnabled()) {
    return false;
  }
  ChromeBrowserState* browser_state = browser->GetBrowserState();
  if (!browser_state) {
    return false;
  }
  search_engines::SearchEngineChoiceService* search_engine_choice_service =
      ios::SearchEngineChoiceServiceFactory::GetForBrowserState(browser_state);
  BrowserStatePolicyConnector* policy_connector =
      browser_state->GetPolicyConnector();
  return search_engine_choice_service->ShouldShowChoiceScreen(
      *policy_connector->GetPolicyService(),
      /*is_regular_profile=*/true,
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state));
}
