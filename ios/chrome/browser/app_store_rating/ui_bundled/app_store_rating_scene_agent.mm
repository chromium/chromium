// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_store_rating/ui_bundled/app_store_rating_scene_agent.h"

#import "base/metrics/histogram_functions.h"
#import "base/ranges/algorithm.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/pref_service.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/app_store_rating/ui_bundled/features.h"

@interface AppStoreRatingSceneAgent ()

// The PromosManager is used to register promos.
@property(nonatomic, assign) PromosManager* promosManager;

@end

@implementation AppStoreRatingSceneAgent

- (instancetype)initWithPromosManager:(PromosManager*)promosManager {
  if (self = [super init]) {
    _promosManager = promosManager;
  }
  return self;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level == SceneActivationLevelForegroundActive) {
    BOOL isUserEngaged = [self isUserEngaged];
    base::UmaHistogramBoolean("IOS.AppStoreRating.UserIsEligible",
                              isUserEngaged);
    if (isUserEngaged) {
      [self requestPromoDisplay];
    }
  }
}

#pragma mark - Private

// Returns true if the Credentials Provider Extension is enabled.
- (BOOL)isCredentialsProviderConditionMet {
  DCHECK(self.sceneState.browserProviderInterface.mainBrowserProvider.browser);
  PrefService* pref_service =
      self.sceneState.browserProviderInterface.mainBrowserProvider.browser
          ->GetBrowserState()
          ->GetPrefs();
  return password_manager_util::IsCredentialProviderEnabledOnStartup(
      pref_service);
}

// Returns true if this is likely the user's default browser and the user is not
// in a country excluded from the default browser eligibility condition.
- (BOOL)isDefaultBrowserConditionMet {
  if (!IsDefaultBrowserConditionExclusionInEffect()) {
    return IsChromeLikelyDefaultBrowser();
  }

  // If for some reason the variations service isn't available to determine the
  // current country, err on the side of caution and assume the country is
  // excluded.
  BOOL countryIsExcluded = true;
  variations::VariationsService* variations_service =
      GetApplicationContext()->GetVariationsService();
  if (variations_service) {
    countryIsExcluded =
        base::Contains(GetCountriesExcludedFromDefaultBrowserCondition(),
                       variations_service->GetStoredPermanentCountry());
  }
  return IsChromeLikelyDefaultBrowser() && !countryIsExcluded;
}

// Returns true if the user is considered engaged for the purpose of the App
// Store rating prompt. A user is considered engaged if they satisfy either the
// default browser or the Credentials Provider Extension condition.
- (BOOL)isUserEngaged {
  return [self isDefaultBrowserConditionMet] ||
         [self isCredentialsProviderConditionMet];
}

// Registers the App Store rating prompt with the promo manager.
- (void)requestPromoDisplay {
  if (!_promosManager || !GetApplicationContext()->GetLocalState()->GetBoolean(
                             prefs::kAppStoreRatingPolicyEnabled)) {
    return;
  }
  _promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::AppStoreRating);
}

@end
