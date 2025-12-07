// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_provider.h"

#import "base/feature_list.h"
#import "base/notreached.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_provider+protected.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_util.h"
#import "ios/chrome/browser/search_engine_choice/ui/search_engine_choice_ui_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

namespace {
// Adds the Default Browser promo to the FRE based on
// kSkipDefaultBrowserPromoInFirstRun.
void AddDBPromoScreen(NSMutableArray* screens, ProfileIOS* profile) {
  regional_capabilities::RegionalCapabilitiesService*
      regional_capabilities_service =
          ios::RegionalCapabilitiesServiceFactory::GetForProfile(profile);

  // Do not display the Default Browser promo if a user is in the EEA and
  // kSkipDefaultBrowserPromoInFirstRun is enabled. Otherwise, display the
  // Default Browser promo.
  if (!(regional_capabilities_service->IsInEeaCountry() &&
        base::FeatureList::IsEnabled(
            first_run::kSkipDefaultBrowserPromoInFirstRun))) {
    [screens addObject:@(kDefaultBrowserPromo)];
  }
}

// Helper function to add the Best Features, Default Browser Promo, and Address
// Bar screens when kUpdatedFirstRunSequence is disabled.
void AddDBPromoAndBestFeaturesScreens(NSMutableArray* screens,
                                      ProfileIOS* profile) {
  using enum first_run::BestFeaturesScreenVariationType;
  first_run::BestFeaturesScreenVariationType bestFeaturesType =
      first_run::GetBestFeaturesScreenVariationType();
  switch (bestFeaturesType) {
    case kGeneralScreenAfterDBPromo:
    case kGeneralScreenWithPasswordItemAfterDBPromo:
    case kShoppingUsersWithFallbackAfterDBPromo:
    case kSignedInUsersOnlyAfterDBPromo:
      AddDBPromoScreen(screens, profile);
      [screens addObject:@(kBestFeatures)];
      break;
    case kGeneralScreenBeforeDBPromo:
      [screens addObject:@(kBestFeatures)];
      AddDBPromoScreen(screens, profile);
      break;
    case kAddressBarPromoInsteadOfBestFeaturesScreen:
      // TODO(crbug.com/402429544): Add address bar promo screen.
      AddDBPromoScreen(screens, profile);
      break;
    case kDisabled:
      AddDBPromoScreen(screens, profile);
      break;
  }
}

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
      // Only add best features screen if feature
      // kUpdatedFirstRunSequence is disabled for now.
      AddDBPromoAndBestFeaturesScreens(screens, profile);
      break;
    case first_run::UpdatedFRESequenceVariationType::kDBPromoFirst:
      AddDBPromoScreen(screens, profile);
      [screens addObject:@(kSignIn)];
      [screens addObject:@(kHistorySync)];
      break;
    case first_run::UpdatedFRESequenceVariationType::kRemoveSignInSync:
      if (hasIdentities) {
        [screens addObject:@(kSignIn)];
        [screens addObject:@(kHistorySync)];
      }
      AddDBPromoScreen(screens, profile);
      break;
    case first_run::UpdatedFRESequenceVariationType::
        kDBPromoFirstAndRemoveSignInSync:
      AddDBPromoScreen(screens, profile);
      if (hasIdentities) {
        [screens addObject:@(kSignIn)];
        [screens addObject:@(kHistorySync)];
      }
      break;
  }

  if (IsBestOfAppLensInteractivePromoEnabled()) {
    [screens addObject:@(kLensInteractivePromo)];
  } else if (IsBestOfAppLensAnimatedPromoEnabled()) {
    [screens addObject:@(kLensAnimatedPromo)];
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
