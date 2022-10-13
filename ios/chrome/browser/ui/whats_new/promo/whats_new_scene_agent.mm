// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/promo/whats_new_scene_agent.h"

#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
    case SceneActivationLevelBackground:
    case SceneActivationLevelForegroundInactive: {
      break;
    }
  }
}

#pragma mark - Private

// Register the What's New promo for a single display in the promo manager.
- (void)registerPromoForSingleDisplay {
  if (IsFullscreenPromosManagerEnabled()) {
    DCHECK(self.promosManager);
    self.promosManager->RegisterPromoForSingleDisplay(
        promos_manager::Promo::WhatsNew);
    setWhatsNewPromoRegistration();
  }
}

@end
