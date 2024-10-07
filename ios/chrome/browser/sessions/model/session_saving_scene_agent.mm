// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_saving_scene_agent.h"

#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@implementation SessionSavingSceneAgent {
  // YES when the scene reached SceneActivationLevelForegroundActive.
  BOOL _sceneWasActivated;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  switch (level) {
    case SceneActivationLevelUnattached:
    case SceneActivationLevelDisconnected:
    case SceneActivationLevelForegroundInactive:
      // no-op.
      break;
    case SceneActivationLevelBackground:
      [self saveSessionsIfNeeded];
      break;
    case SceneActivationLevelForegroundActive:
      _sceneWasActivated = YES;
      break;
  }
}

#pragma mark - Public

- (void)saveSessionsIfNeeded {
  // No need to save the session if the scene didn't reach the
  // SceneActivationLevelForegroundActive stage.
  if (!_sceneWasActivated) {
    return;
  }

  // Forget the scene was activated. Even if the session is not saved, this
  // is okay because we do not really care about change that could happen
  // while the scene is backgrounded. If/when it becomes active again, the
  // flag will be set to YES and on the next background the session will be
  // saved (assuming that by this point the profile initialization is over).
  _sceneWasActivated = NO;

  // If the ProfileState is not ProfileInitStage::kFinal, then do not try
  // to save the session as the profile may not be ready yet.
  ProfileState* profileState = self.sceneState.profileState;
  if (profileState.initStage < ProfileInitStage::kFinal) {
    return;
  }

  // Since the app is about to be backgrounded or terminated, save the sessions
  // immediately for the main and incognito Profiles (if they exists).
  DCHECK(profileState.profile);
  ProfileIOS* mainProfile = profileState.profile;
  SessionRestorationServiceFactory::GetForProfile(mainProfile)->SaveSessions();

  if (mainProfile->HasOffTheRecordProfile()) {
    SessionRestorationServiceFactory::GetForProfile(
        mainProfile->GetOffTheRecordProfile())
        ->SaveSessions();
  }
}

@end
