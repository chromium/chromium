// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_extension/model/share_extension_scene_agent.h"

#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

@interface ShareExtensionSceneAgent () <ProfileStateObserver>
@end

@implementation ShareExtensionSceneAgent

#pragma mark - ObservingSceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];
  [sceneState.profileState addObserver:self];
  [self sceneState:sceneState
      transitionedToActivationLevel:sceneState.activationLevel];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  ProfileState* profileState = self.sceneState.profileState;
  [self transitionToSceneActivationLevel:level
                        profileInitStage:profileState.initStage];
}

#pragma mark - ProfileStateObserver
- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  [self transitionToSceneActivationLevel:self.sceneState.activationLevel
                        profileInitStage:nextInitStage];
}

#pragma mark - Private

// Will be called on profile state transition and scene state transition, the
// aim if to have the scene activation at foreground and the profile state at
// final in order to update the primary account in the shared defaults.
- (void)transitionToSceneActivationLevel:(SceneActivationLevel)level
                        profileInitStage:(ProfileInitStage)profileInitStage {
  if (level != SceneActivationLevelForegroundActive ||
      profileInitStage < ProfileInitStage::kFinal) {
    return;
  }

  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(
          self.sceneState.profileState.profile);
  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
  id<SystemIdentity> identity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (!identity.gaiaId.empty()) {
    [shared_defaults setObject:identity.gaiaId.ToNSString()
                        forKey:app_group::kPrimaryAccount];
  } else {
    [shared_defaults removeObjectForKey:app_group::kPrimaryAccount];
  }
}

@end
