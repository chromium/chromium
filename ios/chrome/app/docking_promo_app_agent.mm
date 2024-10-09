// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/docking_promo_app_agent.h"

#import <optional>

#import "base/check.h"
#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/docking_promo/model/utils.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_util.h"

@interface DockingPromoAppAgent () <AppStateObserver>
@end

@implementation DockingPromoAppAgent {
  // The app state for the app.
  __weak AppState* _appState;
}

#pragma mark - AppStateAgent

- (void)setAppState:(AppState*)appState {
  // `-setAppState:appState` should only be called once!
  CHECK(!_appState);

  _appState = appState;
  [appState addObserver:self];
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  if (_appState.initStage == AppInitStage::kFinal) {
    switch (DockingPromoExperimentTypeEnabled()) {
      case DockingPromoDisplayTriggerArm::kDuringFRE:
        break;
      case DockingPromoDisplayTriggerArm::kAfterFRE:
        if (previousInitStage != AppInitStage::kFirstRun) {
          break;
        }
        [[fallthrough]];
      case DockingPromoDisplayTriggerArm::kAppLaunch:
        [self maybeRegisterPromo];
    }

    [_appState removeObserver:self];
    [_appState removeAgent:self];
  }
}

#pragma mark - Private

// Register the promo with the PromosManager, if the conditions are met.
- (void)maybeRegisterPromo {
  if (IsDockingPromoForcedForDisplay()) {
    [self registerPromo];
    return;
  }

  std::optional<base::TimeDelta> timeSinceLastForeground =
      MinTimeSinceLastForeground(_appState.foregroundScenes);

  if (!CanShowDockingPromo(
          timeSinceLastForeground.value_or(base::TimeDelta::Min()))) {
    if (!base::FeatureList::IsEnabled(
            kIOSDockingPromoPreventDeregistrationKillswitch)) {
      [self deregisterPromo];
    }
    return;
  }

  // Record that the user has at least been found eligible once
  // for the docking promo. This means that it is now possible
  // to use IsDockingPromoForEligibleUsersOnlyEnabled() and be
  // sure that the feature will only be A/B tested on users that
  // are eligible, thus reducing the noise in measurement by not
  // including ineligible users.
  GetApplicationContext()->GetLocalState()->SetBoolean(
      prefs::kIosDockingPromoEligibilityMet, true);

  if (IsDockingPromoForEligibleUsersOnlyEnabled()) {
    [self registerPromo];
  }
}

// Registers the Docking Promo with the PromosManager.
- (void)registerPromo {
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    PromosManager* promosManager = PromosManagerFactory::GetForProfile(profile);
    promosManager->RegisterPromoForSingleDisplay(
        promos_manager::Promo::DockingPromo);
  }
}

// Deregisters the Docking Promo from the PromosManager.
- (void)deregisterPromo {
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    PromosManager* promosManager = PromosManagerFactory::GetForProfile(profile);
    promosManager->DeregisterPromo(promos_manager::Promo::DockingPromo);
  }
}

@end
