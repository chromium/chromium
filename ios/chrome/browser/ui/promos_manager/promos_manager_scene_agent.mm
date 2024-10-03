// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/promos_manager/promos_manager_scene_agent.h"

#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/ui/promos_manager/utils.h"

@interface PromosManagerSceneAgent () <ProfileStateObserver>

// Indicates whether or not the UI is available for a promo to be displayed.
@property(nonatomic, assign, readonly, getter=isUIAvailableForPromo)
    BOOL UIAvailableForPromo;

@end

@implementation PromosManagerSceneAgent

- (instancetype)initWithCommandDispatcher:(CommandDispatcher*)dispatcher {
  DCHECK(ShouldPromoManagerDisplayPromos());
  self = [super init];
  if (self) {
    _dispatcher = dispatcher;
  }
  return self;
}

- (void)maybeForceDisplayPromo {
  [self maybeNotifyObserver];
}

#pragma mark - ObservingSceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];

  [self.sceneState.profileState addObserver:self];
}

#pragma mark - AppStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  // Monitor the profile initialization stages to consider showing a promo at a
  // point in the initialization of the app that allows it.
  [self maybeNotifyObserver];
}

#pragma mark - SceneStateObserver

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  [self.sceneState.profileState removeObserver:self];
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
  if (IsUIAvailableForPromo(self.sceneState)) {
    id<PromosManagerCommands> promosManagerHandler =
        HandlerForProtocol(self.dispatcher, PromosManagerCommands);

    [promosManagerHandler maybeDisplayPromo];
  }
}

@end
