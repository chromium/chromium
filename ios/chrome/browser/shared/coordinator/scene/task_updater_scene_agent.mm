// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/task_updater_scene_agent.h"

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/app/task_orchestrator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

@interface TaskUpdaterSceneAgent () <ProfileStateObserver,
                                     UIBlockerManagerObserver>
@end

@implementation TaskUpdaterSceneAgent

#pragma mark - ObservingSceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];
  [self.sceneState.profileState addObserver:self];
  [self.sceneState.profileState addUIBlockerManagerObserver:self];

  // Make sure that the execution stage is updated also if a scene is connected
  // after the ProfileState has reached stage ProfileInitStage::kProfileLoaded
  // or higher.
  if (self.sceneState.profileState.initStage >=
      ProfileInitStage::kProfileLoaded) {
    [self updateToProfileLoaded];
  }
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage == ProfileInitStage::kProfileLoaded) {
    [self updateToProfileLoaded];
  } else if (nextInitStage == ProfileInitStage::kFinal) {
    [self maybeUpdateToUIReady];
  }
}

#pragma mark - UIBlockerManagerObserver

- (void)currentUIBlockerRemoved {
  [self maybeUpdateToUIReady];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  [self maybeUpdateToUIReady];
}

- (void)sceneStateDidHideModalOverlay:(SceneState*)sceneState {
  [self maybeUpdateToUIReady];
}

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  [self maybeUpdateToUIReady];
}

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  [self.sceneState.profileState removeObserver:self];
  [self.sceneState removeObserver:self];
  [self.sceneState.profileState removeUIBlockerManagerObserver:self];
}

- (void)signinDidEnd:(SceneState*)sceneState {
  // Handle intents after sign-in is done when the forced sign-in policy
  // is enabled.
  [self maybeUpdateToUIReady];
}

#pragma mark - Private

// Updates the scene to TaskExecutionProfileLoaded.
- (void)updateToProfileLoaded {
  [self.sceneState.profileState.appState.taskOrchestrator
      updateToStage:TaskExecutionStage::TaskExecutionProfileLoaded
           forScene:self.sceneState.sceneSessionID];
}

// Updates the scene to TaskExecutionUIReady if conditions are met.
- (void)maybeUpdateToUIReady {
  if (![self canUpdateToUIReady]) {
    return;
  }
  [self.sceneState.profileState.appState.taskOrchestrator
      updateToStage:TaskExecutionStage::TaskExecutionUIReady
           forScene:self.sceneState.sceneSessionID];
}

// YES if UI is ready to handle tasks.
- (BOOL)canUpdateToUIReady {
  SceneState* sceneState = self.sceneState;
  if (!sceneState.UIEnabled) {
    return NO;
  }
  if (sceneState.profileState.currentUIBlocker) {
    return NO;
  }
  if (sceneState.activationLevel < SceneActivationLevelForegroundActive) {
    return NO;
  }
  if (sceneState.presentingModalOverlay) {
    return NO;
  }
  if (sceneState.profileState.initStage < ProfileInitStage::kFinal) {
    return NO;
  }
  if ([self signinStatusInSyncWithPolicy]) {
    return NO;
  }
  return YES;
}

// YES if the UI should wait for an authentication flow forced by policy.
- (BOOL)signinStatusInSyncWithPolicy {
  ProfileIOS* profile = self.sceneState.profileState.profile;
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  if (authService->GetServiceStatus() !=
      AuthenticationService::ServiceStatus::SigninForcedByPolicy) {
    return NO;
  }
  // Wait for sign-in to complete before handling UI tasks.
  if (self.sceneState.signinInProgress) {
    return YES;
  }

  // UI tasks can be handled if there is already a primary account signed in.
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  return !identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

@end
