// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_screen_provider.h"

#import "base/feature_list.h"
#import "base/notreached.h"
#import "components/search_engines/search_engine_choice_utils.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/policy/browser_state_policy_connector.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/screen/screen_provider+protected.h"
#import "ios/chrome/browser/ui/screen/screen_type.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

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

  BrowserStatePolicyConnector* policyConnector =
      browserState->GetPolicyConnector();
  if (ios::provider::IsSearchEngineChoiceScreenEnabledFre() &&
      search_engines::ShouldShowChoiceScreen(
          *policyConnector->GetPolicyService(),
          /*profile_properties=*/
          {.is_regular_profile = true,
           .pref_service = browserState->GetPrefs()},
          ios::TemplateURLServiceFactory::GetForBrowserState(browserState))) {
    [screens addObject:@(kChoice)];
  }

  [screens addObject:@(kDefaultBrowserPromo)];

  if (IsBottomOmniboxPromoFlagEnabled(BottomOmniboxPromoType::kFRE)) {
    [screens addObject:@(kOmniboxPosition)];
  }

  [screens addObject:@(kStepsCompleted)];
  return [super initWithScreens:screens];
}

@end
