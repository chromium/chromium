// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/promos_manager_scene_agent.h"

#import "base/time/time.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/browser/ui/main/scene_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// If the previous app foregrounding occurred less than
// `kMinutesBetweenForegrounding` minutes ago, a promo won't be displayed.
constexpr int kMinutesBetweenForegrounding = 240;  // (4 hours) (inclusive)

}  // namespace

@interface PromosManagerSceneAgent () <AppStateObserver>
@end

@implementation PromosManagerSceneAgent

- (instancetype)init {
  self = [super init];

  return self;
}

#pragma mark - ObservingSceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];

  [self.sceneState.appState addObserver:self];
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  // Monitor the app intialization stages to consider showing a promo at a point
  // in the initialization of the app that allows it.
  [self handlePromoDisplayIfUIAvailable];
}

#pragma mark - SceneStateObserver

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  [self.sceneState.appState removeObserver:self];
  [self.sceneState removeObserver:self];
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  // Monitor the scene activation level to consider showing a promo
  // when the scene becomes active and in the foreground. In which case the
  // scene is visible and interactable.
  [self handlePromoDisplayIfUIAvailable];
}

#pragma mark - Private

// Handle the display of a promo if the scene UI is available to display one.
- (void)handlePromoDisplayIfUIAvailable {
  if (![self isUIAvailableForPromo])
    return;
}

// Returns YES if a promo can be displayed.
- (BOOL)isUIAvailableForPromo {
  // The following app & scene conditions need to be met to enable a promo's
  // display (please note the Promos Manager may still decide *not* to display a
  // promo, based on its own internal criteria):

  // (1) The app initialization is over (the stage InitStageFinal is reached).
  if (self.sceneState.appState.initStage < InitStageFinal)
    return NO;

  // (2) The scene is in the foreground.
  if (self.sceneState.activationLevel < SceneActivationLevelForegroundActive)
    return NO;

  // (3) At least `kMinutesBetweenForegrounding` have elapsed since the app was
  // last foregrounded, if ever.
  if (!self.sceneState.appState.lastTimeInForeground.is_null()) {
    base::TimeDelta elapsedTimeSinceLastForeground =
        base::TimeTicks::Now() - self.sceneState.appState.lastTimeInForeground;

    if (elapsedTimeSinceLastForeground.InMinutes() <
        kMinutesBetweenForegrounding)
      return NO;
  }

  // (4) There is no UI blocker.
  if (self.sceneState.appState.currentUIBlocker)
    return NO;

  // (5) The app isn't shutting down.
  if (self.sceneState.appState.appIsTerminating)
    return NO;

  // (6) There are no launch intents (external intents).
  if (self.sceneState.startupHadExternalIntent)
    return NO;

  // (7) The app isn't launching after a crash.
  if (self.sceneState.appState.postCrashLaunch)
    return NO;

  // Additional, sensible checks to add to minimize user annoyance:

  // (8) The user isn't currently signing in.
  if (self.sceneState.signinInProgress)
    return NO;

  // (9) The user isn't currently looking at a modal overlay.
  if (self.sceneState.presentingModalOverlay)
    return NO;

  return YES;
}

@end
