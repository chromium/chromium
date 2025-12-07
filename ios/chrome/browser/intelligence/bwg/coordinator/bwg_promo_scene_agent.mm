// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_promo_scene_agent.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"

@implementation BWGPromoSceneAgent {
  raw_ptr<PromosManager, DanglingUntriaged> _promosManager;
}

- (instancetype)initWithPromosManager:(PromosManager*)promosManager {
  self = [super init];
  if (self) {
    _promosManager = promosManager;
  }
  return self;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  switch (level) {
    case SceneActivationLevelForegroundActive: {
      if (IsPageActionMenuEnabled()) {
        [self registerPromoForSingleDisplay];
      }
      break;
    }
    case SceneActivationLevelUnattached:
    case SceneActivationLevelBackground:
    case SceneActivationLevelDisconnected:
    case SceneActivationLevelForegroundInactive: {
      break;
    }
  }
}

#pragma mark - Private

// Register the What's New promo for a single display in the promo manager.
- (void)registerPromoForSingleDisplay {
  DCHECK(_promosManager);
  _promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::BWGPromo);
}

@end
