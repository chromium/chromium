// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/app_metrics_app_state_agent.h"

#import "base/time/time.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/public/provider/chrome/browser/primes/primes_api.h"

namespace {
// Constant for deferring snapshotting startup memory usage
NSString* const kTakeStartupMemorySnapshot = @"TakeStartupMemorySnapshot";
// Constant for naming the startup memory snapshot
NSString* const kDeferredInitializationBlocksComplete =
    @"DeferredInitializationBlocksComplete";
}  // namespace

@implementation AppMetricsAppStateAgent {
  // This flag is set when the first scene has connected since the startup, and
  // never reset during the app's lifetime.
  BOOL _firstSceneHasConnected;

  // This flag is set when the first scene has activated since the startup, and
  // never reset during the app's lifetime.
  BOOL _firstSceneHasActivated;
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState {
  [super appState:appState sceneConnected:sceneState];

  if (!_firstSceneHasConnected) {
    _firstSceneHasConnected = YES;
    self.appState.startupInformation.firstSceneConnectionTime =
        base::TimeTicks::Now();

    const AppInitStage initStage = self.appState.initStage;
    if (initStage >= AppInitStage::kBrowserObjectsForBackgroundHandlers) {
      [self firstSceneConnected];
    }
  }
}

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  [super appState:appState didTransitionFromInitStage:previousInitStage];

  const AppInitStage initStage = self.appState.initStage;
  if (initStage == AppInitStage::kBrowserObjectsForBackgroundHandlers) {
    if (_firstSceneHasConnected) {
      [self firstSceneConnected];
    }

    if (_firstSceneHasActivated) {
      [self firstSceneActivated];
    }
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  [super sceneState:sceneState transitionedToActivationLevel:level];

  if (level == SceneActivationLevelForegroundActive) {
    if (!_firstSceneHasActivated) {
      _firstSceneHasActivated = YES;

      const AppInitStage initStage = self.appState.initStage;
      if (initStage >= AppInitStage::kBrowserObjectsForBackgroundHandlers) {
        [self firstSceneActivated];
      }
    }
  }
}

#pragma mark - Private methods

- (void)firstSceneConnected {
  [MetricsMediator createStartupTrackingTask];
}

- (void)firstSceneActivated {
  [MetricsMediator logStartupDuration:self.appState.startupInformation];
  if (ios::provider::IsPrimesSupported()) {
    ios::provider::PrimesAppReady();
  }
  [self.appState.deferredRunner
      enqueueBlockNamed:kTakeStartupMemorySnapshot
                  block:^{
                    if (ios::provider::IsPrimesSupported()) {
                      ios::provider::PrimesTakeMemorySnapshot(
                          kDeferredInitializationBlocksComplete);
                      tests_hook::SignalAppLaunched();
                    }
                    [MetricsMediator
                        logMemoryToUMA:
                            "Memory.Browser.MemoryFootprint.Startup"];
                  }];
}

@end
