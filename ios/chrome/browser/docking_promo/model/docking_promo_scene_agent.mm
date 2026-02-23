// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/model/docking_promo_scene_agent.h"

#import <algorithm>

#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/app/profile/profile_state.h"
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
    ProfileIOS* profile = sceneState.profileState.profile;
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForProfile(profile);

    if (!tracker) {
      return;
    }

    unsigned int chromeOpenedCount = 0;
    std::vector<std::pair<feature_engagement::EventConfig, int>> events =
        tracker->ListEvents(
            feature_engagement::kIPHiOSDockingPromoEligibilityFeature);

    for (const auto& event : events) {
      if (event.first.name == feature_engagement::events::kChromeOpened) {
        chromeOpenedCount++;
      }
    }

    //  Low engaged users (L7 days active <=1)
    BOOL isLowEngagementUser = chromeOpenedCount <= 1;

    // TODO(crbug.com/479220063): use a new kChromeOpenedFromIcon event and
    // use it to check if no app icon launches in last 7 days.
    BOOL hasNoRecentIconLaunches = false;

    if (isLowEngagementUser || hasNoRecentIconLaunches) {
      _promosManager->RegisterPromoForContinuousDisplay(
          promos_manager::Promo::DockingPromo);
    } else {
      _promosManager->DeregisterPromo(promos_manager::Promo::DockingPromo);
    }
  }
}

@end
