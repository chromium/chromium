// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/docking_promo_app_agent.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/docking_promo/model/utils.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"

@interface DockingPromoAppAgent () <AppStateObserver>
@end

@implementation DockingPromoAppAgent {
  // The app state for the app.
  __weak AppState* _appState;

  // Stores the PromosManager, which is used to register the Docking Promo, when
  // appropriate.
  raw_ptr<PromosManager> _promosManager;
}

#pragma mark - Initializers

- (instancetype)initWithPromosManager:(PromosManager*)promosManager {
  if ([super init]) {
    _promosManager = promosManager;
  }

  return self;
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

// Register the promo with the PromosManager, if the conditions are met.
- (void)maybeRegisterPromo {
  if (IsDockingPromoForcedForDisplay()) {
    [self registerPromo];
    return;
  }

  // If the app was never foregrounded, do not register the Docking Promo.
  if (_appState.lastTimeInForeground.is_null()) {
    return;
  }

  base::TimeDelta timeSinceLastForeground =
      _appState.lastTimeInForeground - base::TimeTicks::Now();

  if (!CanShowDockingPromo(timeSinceLastForeground)) {
    [self deregisterPromo];
    return;
  }

  [self registerPromo];
}

// Registers the Docking Promo with the PromosManager.
- (void)registerPromo {
  CHECK(_promosManager);

  _promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::DockingPromo);
}

// Deregisters the Docking Promo from the PromosManager.
- (void)deregisterPromo {
  CHECK(_promosManager);

  _promosManager->DeregisterPromo(promos_manager::Promo::DockingPromo);
}

@end
