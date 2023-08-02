// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/promos_manager/promos_manager_scene_agent.h"

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"

@interface PromosManagerSceneAgent () <AppStateObserver>

// Indicates whether or not the UI is available for a promo to be displayed.
@property(nonatomic, assign, readonly, getter=isUIAvailableForPromo)
    BOOL UIAvailableForPromo;

@end

@implementation PromosManagerSceneAgent

- (instancetype)initWithCommandDispatcher:(CommandDispatcher*)dispatcher {
  self = [super init];
  if (self) {
    _dispatcher = dispatcher;
  }
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
  [self maybeNotifyObserver];
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
  [self maybeNotifyObserver];
}

#pragma mark - Private

// Notify observer(s) that the UI is available for a promo.
- (void)maybeNotifyObserver {
  if (self.UIAvailableForPromo) {
    id<PromosManagerCommands> promosManagerHandler =
        HandlerForProtocol(self.dispatcher, PromosManagerCommands);

    [promosManagerHandler maybeDisplayPromo];
  }
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

  // (3) There is no UI blocker.
  if (self.sceneState.appState.currentUIBlocker)
    return NO;

  // (4) The app isn't shutting down.
  if (self.sceneState.appState.appIsTerminating)
    return NO;

  // (5) There are no launch intents (external intents).
  if (self.sceneState.startupHadExternalIntent)
    return NO;

  // Additional, sensible checks to add to minimize user annoyance:

  // (6) The user isn't currently signing in.
  if (self.sceneState.signinInProgress)
    return NO;

  // (7) The user isn't currently looking at a modal overlay.
  if (self.sceneState.presentingModalOverlay)
    return NO;

  return YES;
}

@end
