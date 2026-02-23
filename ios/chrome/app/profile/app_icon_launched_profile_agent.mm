// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/app_icon_launched_profile_agent.h"

#import <UserNotifications/UserNotifications.h>

#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/observing_app_state_agent.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@implementation AppIconLaunchedProfileAgent {
  // Tracks if the transition to ForegroundActive has been handled for any scene
  // in this profile session. Prevents multiple notifications when multiple
  // scenes exist (e.g., on iPad).
  BOOL _foregroundActiveEventAlreadyHandled;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  switch (level) {
    case SceneActivationLevelBackground:
    case SceneActivationLevelForegroundInactive:
    case SceneActivationLevelDisconnected:
    case SceneActivationLevelUnattached:
      // Reset the flag when no scene is fully active.
      _foregroundActiveEventAlreadyHandled = NO;
      break;

    case SceneActivationLevelForegroundActive:
      if (!_foregroundActiveEventAlreadyHandled &&
          !sceneState.startupHadExternalIntent) {
        _foregroundActiveEventAlreadyHandled = YES;

        __weak __typeof(self) weakSelf = self;
        void (^initializationHandler)(bool) = ^(bool successfullyLoaded) {
          if (successfullyLoaded) {
            [weakSelf notifyChromeOpenedViaIcon];
          }
        };

        ProfileState* profileState = sceneState.profileState;
        feature_engagement::Tracker* tracker =
            feature_engagement::TrackerFactory::GetForProfile(
                profileState.profile);
        tracker->AddOnInitializedCallback(
            base::BindOnce(initializationHandler));
      }
      break;
  }
}

#pragma mark - Private

// Notifies the Feature Engagement Tracker that the Chrome app has been opened
// via the home screen icon.
- (void)notifyChromeOpenedViaIcon {
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(
          self.profileState.profile);

  CHECK(tracker && tracker->IsInitialized(), base::NotFatalUntil::M150);

  tracker->NotifyEvent(feature_engagement::events::kIOSChromeOpenedFromIcon);
}

@end
