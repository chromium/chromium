// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/docking_promo_app_agent.h"

#import <optional>

#import "base/check.h"
#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/docking_promo/model/utils.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
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
    didTransitionFromInitStage:(InitStage)previousInitStage {
  if (_appState.initStage == InitStageFinal) {
    switch (DockingPromoExperimentTypeEnabled()) {
      case DockingPromoDisplayTriggerArm::kDuringFRE:
        break;
      case DockingPromoDisplayTriggerArm::kAfterFRE:
        if (previousInitStage != InitStageFirstRun) {
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

  [self registerPromo];
}

// Registers the Docking Promo with the PromosManager.
- (void)registerPromo {
  std::vector<ChromeBrowserState*> loadedBrowserStates =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();
  for (ChromeBrowserState* browserState : loadedBrowserStates) {
    PromosManager* promosManager =
        PromosManagerFactory::GetForBrowserState(browserState);
    promosManager->RegisterPromoForSingleDisplay(
        promos_manager::Promo::DockingPromo);
  }
}

// Deregisters the Docking Promo from the PromosManager.
- (void)deregisterPromo {
  std::vector<ChromeBrowserState*> loadedBrowserStates =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();
  for (ChromeBrowserState* browserState : loadedBrowserStates) {
    PromosManager* promosManager =
        PromosManagerFactory::GetForBrowserState(browserState);
    promosManager->DeregisterPromo(promos_manager::Promo::DockingPromo);
  }
}

@end
