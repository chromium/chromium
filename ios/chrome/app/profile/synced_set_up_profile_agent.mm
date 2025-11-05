// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/synced_set_up_profile_agent.h"

#import "base/check.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/connection_information.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/synced_set_up_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/synced_set_up/utils/utils.h"

@interface SyncedSetUpProfileAgent ()
@end

@implementation SyncedSetUpProfileAgent {
  // Ensures the Synced Set Up confirmation is triggered only once per
  // foreground activation cycle. This prevents duplicate UIs in multi-window
  // scenarios (e.g., iPad split-screen).
  BOOL _activationAlreadyHandled;
}

#pragma mark - SceneObservingProfileAgent

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  switch (level) {
    case SceneActivationLevelDisconnected:
    case SceneActivationLevelUnattached:
    case SceneActivationLevelBackground:
    case SceneActivationLevelForegroundInactive:
      // Reset when scenes become inactive or backgrounded, allowing the logic
      // to run again upon the next foreground activation.
      _activationAlreadyHandled = NO;
      break;
    case SceneActivationLevelForegroundActive:
      // Try triggering when a scene becomes active. Preconditions are checked
      // in `-maybeTriggerSyncedSetUp`.
      [self maybeTriggerSyncedSetUp];
      break;
  }
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage == ProfileInitStage::kFinal) {
    [self maybeTriggerSyncedSetUp];
  }
}

#pragma mark - Private

// Evaluates all preconditions and triggers the Synced Set Up flow if
// applicable.
- (void)maybeTriggerSyncedSetUp {
  CHECK(IsSyncedSetUpEnabled());

  if (_activationAlreadyHandled) {
    return;
  }

  // This agent must not initiate the Synced Set Up flow during First Run.
  if (self.profileState.appState.startupInformation.isFirstRun) {
    return;
  }

  SceneState* activeScene = GetEligibleSceneForSyncedSetUp(self.profileState);

  if (!activeScene) {
    return;
  }

  CommandDispatcher* dispatcher =
      activeScene.browserProviderInterface.mainBrowserProvider.browser
          ->GetCommandDispatcher();
  id<SyncedSetUpCommands> handler =
      HandlerForProtocol(dispatcher, SyncedSetUpCommands);

  if (handler) {
    [handler showSyncedSetUpWithDismissalCompletion:nil];
    _activationAlreadyHandled = YES;
  }
}

@end
