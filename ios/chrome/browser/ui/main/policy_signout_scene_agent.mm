// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/policy_signout_scene_agent.h"

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/policy_change_commands.h"

@interface PolicySignoutSceneAgent ()

// Command Dispatcher.
@property(nonatomic, weak) CommandDispatcher* dispatcher;

@end

@implementation PolicySignoutSceneAgent

- (instancetype)initWithCommandDispatcher:(CommandDispatcher*)dispatcher {
  if ([super init])
    _dispatcher = dispatcher;
  return self;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  AppState* appState = self.sceneState.appState;
  // Can only present UI when activation level is
  // SceneActivationLevelForegroundActive. Show the sign-out prompt if the user
  // was signed out due to policy.
  if (level == SceneActivationLevelForegroundActive &&
      appState.shouldShowPolicySignoutPrompt && !appState.currentUIBlocker) {
    [HandlerForProtocol(self.dispatcher, PolicyChangeCommands)
        showPolicySignoutPrompt];
    appState.shouldShowPolicySignoutPrompt = NO;
  }
}

@end
