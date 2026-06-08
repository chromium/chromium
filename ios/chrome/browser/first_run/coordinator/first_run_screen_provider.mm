// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/coordinator/first_run_screen_provider.h"

#import "base/feature_list.h"
#import "base/notreached.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/first_run/public/features.h"
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

// Helper function to add the Best Features, Default Browser Promo, and Address
// Bar screens when kUpdatedFirstRunSequence is disabled.
void AddDBPromoAndBestFeaturesScreens(NSMutableArray* screens) {
  using enum first_run::BestFeaturesScreenVariationType;
  first_run::BestFeaturesScreenVariationType bestFeaturesType =
      first_run::GetBestFeaturesScreenVariationType();
  switch (bestFeaturesType) {
    case kGeneralScreenAfterDBPromo:
    case kGeneralScreenWithPasswordItemAfterDBPromo:
    case kShoppingUsersWithFallbackAfterDBPromo:
    case kSignedInUsersOnlyAfterDBPromo:
      [screens addObject:@(kDefaultBrowserPromo)];
      [screens addObject:@(kBestFeatures)];
      break;
    case kGeneralScreenBeforeDBPromo:
      [screens addObject:@(kBestFeatures)];
      [screens addObject:@(kDefaultBrowserPromo)];
      break;
    case kAddressBarPromoInsteadOfBestFeaturesScreen:
      // TODO(crbug.com/402429544): Add address bar promo screen.
      [screens addObject:@(kDefaultBrowserPromo)];
      break;
    case kDisabled:
    case kBestOfApp:
      [screens addObject:@(kDefaultBrowserPromo)];
      break;
  }
}

NSArray* FirstRunScreenSequenceForProfile(ProfileIOS* profile) {
  NSMutableArray* screens = [NSMutableArray array];

  BOOL shouldDisplayChoiceScreen = ShouldDisplaySearchEngineChoiceScreen(
      *profile, /*is_first_run_entrypoint=*/true,
      /*app_started_via_external_intent=*/false);

  first_run::UpdatedFRESequenceVariationType variationType =
      shouldDisplayChoiceScreen
          ? first_run::UpdatedFRESequenceVariationType::kDisabled
          : first_run::GetUpdatedFRESequenceVariation();

  BOOL hasIdentities =
      ChromeAccountManagerServiceFactory::GetForProfile(profile)
          ->HasIdentities();

  switch (variationType) {
    case first_run::UpdatedFRESequenceVariationType::kDisabled:
      [screens addObject:@(kSignIn)];
      [screens addObject:@(kHistorySync)];
      if (shouldDisplayChoiceScreen) {
        [screens addObject:@(kChoice)];
      }
      // Only add best features screen if feature
      // kUpdatedFirstRunSequence is disabled for now.
      AddDBPromoAndBestFeaturesScreens(screens);
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

  if (IsBestOfAppLensInteractivePromoEnabled()) {
    [screens addObject:@(kLensInteractivePromo)];
  } else if (IsBestOfAppLensAnimatedPromoEnabled()) {
    [screens addObject:@(kLensAnimatedPromo)];
  } else if (IsBestOfAppBestFeaturesEnabled()) {
    [screens addObject:@(kBestFeatures)];
  }

  // Conditionally remove the Default Browser promo if it's skipped and there
  // is a sign-in screen in the sequence. If the sign-in screen is removed,
  // do not remove the DB screen. At least one of these two screens must be
  // shown because we need the TOS disclaimer to be displayed.
  regional_capabilities::RegionalCapabilitiesService*
      regional_capabilities_service =
          ios::RegionalCapabilitiesServiceFactory::GetForProfile(profile);
  if (first_run::IsSkipDefaultBrowserPromoInFirstRunEnabled(
          regional_capabilities_service->IsInEeaCountry()) &&
      [screens containsObject:@(kSignIn)]) {
    [screens removeObject:@(kDefaultBrowserPromo)];
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
