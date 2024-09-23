// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/promo/whats_new_scene_agent.h"

#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"

@interface WhatsNewSceneAgent ()

@property(nonatomic, assign) PromosManager* promosManager;

@end

@implementation WhatsNewSceneAgent

- (instancetype)initWithPromosManager:(PromosManager*)promosManager {
  self = [super init];
  if (self) {
    self.promosManager = promosManager;
  }
  return self;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  switch (level) {
    case SceneActivationLevelForegroundActive: {
      if (WasWhatsNewUsed()) {
        return;
      }
      DCHECK(self.promosManager);
      self.promosManager->RegisterPromoForContinuousDisplay(
          promos_manager::Promo::WhatsNew);
      break;
    }
    case SceneActivationLevelUnattached:
    case SceneActivationLevelDisconnected:
      break;
    case SceneActivationLevelBackground: {
      id<WhatsNewCommands> handler = HandlerForProtocol(
          sceneState.browserProviderInterface.mainBrowserProvider.browser
              ->GetCommandDispatcher(),
          WhatsNewCommands);
      DCHECK(handler);
      [handler dismissWhatsNew];
      break;
    }
    case SceneActivationLevelForegroundInactive: {
      break;
    }
  }
}

@end
