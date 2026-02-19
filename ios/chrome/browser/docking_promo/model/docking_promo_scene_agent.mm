// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/model/docking_promo_scene_agent.h"

#import <algorithm>

#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/docking_promo/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@interface DockingPromoSceneAgent ()

// The PromosManager is used to register promos.
@property(nonatomic, assign) PromosManager* promosManager;

@end

@implementation DockingPromoSceneAgent

- (instancetype)initWithPromosManager:(PromosManager*)promosManager {
  if ((self = [super init])) {
    _promosManager = promosManager;
  }
  return self;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (!IsDockingPromoV2Enabled()) {
    return;
  }

  if (level == SceneActivationLevelForegroundActive) {
    IOSDockingPromoEligibility eligibility =
        DockingPromoEligibility(sceneState.profileState.profile);

    if (eligibility == IOSDockingPromoEligibility::kIneligible) {
      _promosManager->DeregisterPromo(promos_manager::Promo::DockingPromo);
    } else {
      _promosManager->RegisterPromoForContinuousDisplay(
          promos_manager::Promo::DockingPromo);
    }
  }
}

@end
