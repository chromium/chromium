// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/coordinator/first_run_screen_provider.h"

#import <utility>
#import <vector>

#import "base/notreached.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_provider+protected.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_util.h"
#import "ios/chrome/browser/search_engine_choice/ui/search_engine_choice_ui_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

namespace {

// Returns true if `screen_type` should be skipped during First Run and deferred
// for post-First Run presentation under `ablation_type`.
bool ShouldSkipAndDeferScreen(
    ScreenType screen_type,
    first_run::FREAblationVariationType ablation_type) {
  using enum ScreenType;
  switch (ablation_type) {
    case first_run::FREAblationVariationType::kSkipHistorySync:
      return screen_type == kHistorySync;
    case first_run::FREAblationVariationType::kSkipAllExceptSignInAndChoice:
      switch (screen_type) {
        case kSignIn:
        case kChoice:
        case kStepsCompleted:
          return false;
        case kHistorySync:
        case kDefaultBrowserPromo:
        case kBestFeatures:
        case kLensInteractivePromo:
        case kLensAnimatedPromo:
          return true;
        case kSyncedSetUp:
        case kGuidedTour:
        case kSafariImport:
          NOTREACHED();
      }
  }
}

// Filters `screens` according to the active `kFREAblation` variation and
// persists skipped screens in sequence order to `kFirstRunDeferredScreens`.
void ApplyFREAblationIfNeeded(std::vector<ScreenType>& screens) {
  if (!first_run::IsFREAblationEnabled()) {
    return;
  }
  first_run::FREAblationVariationType ablation_type =
      first_run::GetFREAblationVariationType();

  std::vector<ScreenType> remaining_screens;
  remaining_screens.reserve(screens.size());
  base::ListValue deferred_screens;
  for (ScreenType screen_type : screens) {
    if (ShouldSkipAndDeferScreen(screen_type, ablation_type)) {
      deferred_screens.Append(static_cast<int>(screen_type));
    } else {
      remaining_screens.push_back(screen_type);
    }
  }

  screens = std::move(remaining_screens);
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  if (local_state) {
    local_state->SetList(prefs::kFirstRunDeferredScreens,
                         std::move(deferred_screens));
    // TODO(crbug.com/548482305): Register the deferred First Run promo with
    // `PromosManager` here once `Promo::DeferredFirstRun` is added.
  }
}

// Helper function to add the Best Features, Default Browser Promo, and Address
// Bar screens.
void AddDBPromoAndBestFeaturesScreens(std::vector<ScreenType>& screens) {
  using enum first_run::BestFeaturesScreenVariationType;
  first_run::BestFeaturesScreenVariationType bestFeaturesType =
      first_run::GetBestFeaturesScreenVariationType();
  switch (bestFeaturesType) {
    case kGeneralScreenAfterDBPromo:
    case kGeneralScreenWithPasswordItemAfterDBPromo:
    case kSignedInUsersOnlyAfterDBPromo:
      screens.push_back(ScreenType::kDefaultBrowserPromo);
      screens.push_back(ScreenType::kBestFeatures);
      break;
    case kGeneralScreenBeforeDBPromo:
      screens.push_back(ScreenType::kBestFeatures);
      screens.push_back(ScreenType::kDefaultBrowserPromo);
      break;
    case kAddressBarPromoInsteadOfBestFeaturesScreen:
      // TODO(crbug.com/402429544): Add address bar promo screen.
      screens.push_back(ScreenType::kDefaultBrowserPromo);
      break;
    case kDisabled:
    case kBestOfApp:
      screens.push_back(ScreenType::kDefaultBrowserPromo);
      break;
  }
}

std::vector<ScreenType> FirstRunScreenSequenceForProfile(ProfileIOS* profile) {
  std::vector<ScreenType> screens;

  // Clear any stale deferred screens from an interrupted First Run attempt
  // without querying `base::FeatureList`.
  if (PrefService* local_state = GetApplicationContext()->GetLocalState()) {
    local_state->ClearPref(prefs::kFirstRunDeferredScreens);
  }

  BOOL shouldDisplayChoiceScreen = ShouldDisplaySearchEngineChoiceScreen(
      *profile, /*is_first_run_entrypoint=*/true,
      /*app_started_via_external_intent=*/false);

  BOOL hasIdentities =
      ChromeAccountManagerServiceFactory::GetForProfile(profile)
          ->HasIdentities();

  if (hasIdentities || shouldDisplayChoiceScreen) {
    screens.push_back(ScreenType::kSignIn);
    screens.push_back(ScreenType::kHistorySync);
    if (shouldDisplayChoiceScreen) {
      screens.push_back(ScreenType::kChoice);
    }
  }
  AddDBPromoAndBestFeaturesScreens(screens);

  if (IsBestOfAppLensInteractivePromoEnabled()) {
    screens.push_back(ScreenType::kLensInteractivePromo);
  } else if (IsBestOfAppLensAnimatedPromoEnabled()) {
    screens.push_back(ScreenType::kLensAnimatedPromo);
  } else if (IsBestOfAppBestFeaturesEnabled()) {
    screens.push_back(ScreenType::kBestFeatures);
  }

  // Only evaluate `kFREAblation` when the device has at least one identity so
  // `sticky_starts_active` Finch enrollment is restricted to users who see the
  // Sign-in screen (and accept Terms of Service there).
  if (hasIdentities) {
    ApplyFREAblationIfNeeded(screens);
  }

  screens.push_back(ScreenType::kStepsCompleted);
  return screens;
}

}  // namespace

@implementation FirstRunScreenProvider

- (instancetype)initForProfile:(ProfileIOS*)profile {
  return [super initWithScreens:FirstRunScreenSequenceForProfile(profile)];
}

@end
