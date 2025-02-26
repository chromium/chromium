// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_provider.h"

#import "base/feature_list.h"
#import "base/notreached.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/coordinator/best_features_screen_coordinator.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_provider+protected.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_util.h"
#import "ios/chrome/browser/search_engine_choice/ui_bundled/search_engine_choice_ui_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

namespace {

NSArray* FirstRunScreenSequenceForProfile(ProfileIOS* profile) {
  NSMutableArray* screens = [NSMutableArray array];

  first_run::UpdatedFRESequenceVariationType variationType =
      first_run::GetUpdatedFRESequenceVariation(profile);
  BOOL hasIdentities =
      ChromeAccountManagerServiceFactory::GetForProfile(profile)
          ->HasIdentities();

  switch (variationType) {
    case first_run::UpdatedFRESequenceVariationType::kDisabled:
      [screens addObject:@(kSignIn)];
      [screens addObject:@(kHistorySync)];
      if (ShouldDisplaySearchEngineChoiceScreen(
              *profile, /*is_first_run_entrypoint=*/true,
              /*app_started_via_external_intent=*/false)) {
        [screens addObject:@(kChoice)];
      }
      [screens addObject:@(kDefaultBrowserPromo)];
      // Only add best features screen if feature kUpdatedFirstRunSequence is
      // disabled for now.
      if (base::FeatureList::IsEnabled(
              first_run::kBestFeaturesScreenInFirstRun)) {
        [screens addObject:@(kBestFeatures)];
      }
      break;
    case first_run::UpdatedFRESequenceVariationType::kDBPromoFirst:
      [screens addObject:@(kDefaultBrowserPromo)];
      [screens addObject:@(kSignIn)];
      [screens addObject:@(kHistorySync)];
      break;
    case first_run::UpdatedFRESequenceVariationType::kRemoveSignInSync:
      if (hasIdentities) {
        [screens addObject:@(kSignIn)];
        [screens addObject:@(kHistorySync)];
      }
      [screens addObject:@(kDefaultBrowserPromo)];
      break;
    case first_run::UpdatedFRESequenceVariationType::
        kDBPromoFirstAndRemoveSignInSync:
      [screens addObject:@(kDefaultBrowserPromo)];
      if (hasIdentities) {
        [screens addObject:@(kSignIn)];
        [screens addObject:@(kHistorySync)];
      }
      break;
  }

  DockingPromoDisplayTriggerArm experimentArm =
      DockingPromoExperimentTypeEnabled();

  if (IsDockingPromoEnabled() &&
      experimentArm == DockingPromoDisplayTriggerArm::kDuringFRE) {
    [screens addObject:@(kDockingPromo)];
  }

  [screens addObject:@(kStepsCompleted)];
  return screens;
}

}  // namespace

@implementation FirstRunScreenProvider

- (instancetype)initForProfile:(ProfileIOS*)profile {
  return [super initWithScreens:FirstRunScreenSequenceForProfile(profile)];
}

@end
