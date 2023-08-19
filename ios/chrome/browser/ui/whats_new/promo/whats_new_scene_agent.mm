// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/promo/whats_new_scene_agent.h"

#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
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
      if (ShouldRegisterWhatsNewPromo()) {
        [self registerPromoForSingleDisplay];
      }
      break;
    }
    case SceneActivationLevelUnattached:
    case SceneActivationLevelDisconnected:
      break;
    case SceneActivationLevelBackground: {
      id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
          sceneState.browserProviderInterface.mainBrowserProvider.browser
              ->GetCommandDispatcher(),
          BrowserCoordinatorCommands);
      DCHECK(handler);
      [handler dismissWhatsNew];
      break;
    }
    case SceneActivationLevelForegroundInactive: {
      break;
    }
  }
}

#pragma mark - Private

// Register the What's New promo for a single display in the promo manager.
- (void)registerPromoForSingleDisplay {
  DCHECK(self.promosManager);

  self.promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::WhatsNew);

  setWhatsNewPromoRegistration();
}

@end
