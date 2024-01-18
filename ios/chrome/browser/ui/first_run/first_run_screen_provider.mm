// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_screen_provider.h"

#import "base/feature_list.h"
#import "base/notreached.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engine_choice_utils.h"
#import "components/sync/base/features.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/policy/model/browser_state_policy_connector.h"
#import "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_util.h"
#import "ios/chrome/browser/ui/screen/screen_provider+protected.h"
#import "ios/chrome/browser/ui/screen/screen_type.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_ui_util.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

namespace ios {
namespace first_run {

bool IsSearchEngineChoiceScreenEnabledFre() {
  if (IsSearchEngineForceEnabled()) {
    // This branch is only selected in tests that are related to choice screen.
    return true;
  }
  if (tests_hook::DisableDefaultSearchEngineChoice()) {
    // This branch is taken in every other tests.
    return false;
  }
  if (ios::provider::DisableDefaultSearchEngineChoice()) {
    // Outside of tests, this view should be disabled upstream.
    return false;
  }
  return search_engines::IsChoiceScreenFlagEnabled(
      search_engines::ChoicePromo::kFre);
}
}  // namespace first_run
}  // namespace ios

@implementation FirstRunScreenProvider

- (instancetype)initForBrowserState:(ChromeBrowserState*)browserState {
  NSMutableArray* screens = [NSMutableArray array];
  [screens addObject:@(kSignIn)];
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    [screens addObject:@(kHistorySync)];
  } else {
    [screens addObject:@(kTangibleSync)];
  }

  search_engines::SearchEngineChoiceService* search_engine_choice_service =
      ios::SearchEngineChoiceServiceFactory::GetForBrowserState(browserState);
  BrowserStatePolicyConnector* policyConnector =
      browserState->GetPolicyConnector();
  if (ios::first_run::IsSearchEngineChoiceScreenEnabledFre() &&
      search_engine_choice_service->ShouldShowChoiceScreen(
          *policyConnector->GetPolicyService(),
          /*is_regular_profile=*/true,
          ios::TemplateURLServiceFactory::GetForBrowserState(browserState))) {
    [screens addObject:@(kChoice)];
  }

  [screens addObject:@(kDefaultBrowserPromo)];

  if (IsBottomOmniboxPromoFlagEnabled(BottomOmniboxPromoType::kFRE) &&
      ShouldShowOmniboxPositionChoiceInFRE(browserState)) {
    [screens addObject:@(kOmniboxPosition)];
  }

  [screens addObject:@(kStepsCompleted)];
  return [super initWithScreens:screens];
}

@end
