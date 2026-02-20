// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/picture_in_picture/model/picture_in_picture_scene_agent.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/picture_in_picture_commands.h"

@interface PictureInPictureSceneAgent ()
@end

@implementation PictureInPictureSceneAgent

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  switch (level) {
    case SceneActivationLevelForegroundActive: {
      id<PictureInPictureCommands> handler = HandlerForProtocol(
          sceneState.browserProviderInterface.currentBrowserProvider.browser
              ->GetCommandDispatcher(),
          PictureInPictureCommands);
      [handler dismissPictureInPictureIfNotPipRestore];
      break;
    }
    case SceneActivationLevelUnattached:
    case SceneActivationLevelDisconnected:
    case SceneActivationLevelBackground:
    case SceneActivationLevelForegroundInactive: {
      break;
    }
  }
}

@end
