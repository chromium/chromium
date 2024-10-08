// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/profile_controller.h"

#import <memory>

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/post_restore_profile_agent.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/app/search_engine_choice_profile_agent.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_profile_agent.h"
#import "ios/chrome/browser/profile_metrics/model/profile_activity_profile_agent.h"
#import "ios/chrome/browser/ui/device_orientation/scoped_force_portrait_orientation.h"

@interface ProfileController () <ProfileStateObserver>
@end

@implementation ProfileController {
  // Used to force the device orientation in portrait mode on iPhone.
  std::unique_ptr<ScopedForcePortraitOrientation> _scopedForceOrientation;
}

- (instancetype)initWithAppState:(AppState*)appState {
  if ((self = [super init])) {
    _state = [[ProfileState alloc] initWithAppState:appState];
    _scopedForceOrientation = ForcePortraitOrientationOnIphone(appState);
    [_state addObserver:self];
  }
  return self;
}

#pragma mark ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  switch (nextInitStage) {
    case ProfileInitStage::kStart:
      break;

    case ProfileInitStage::kLoadProfile:
      break;

    case ProfileInitStage::kProfileLoaded:
      [self attachProfileAgents];
      break;

    case ProfileInitStage::kEnterprise:
      break;

    case ProfileInitStage::kPrepareUI:
      break;

    case ProfileInitStage::kUIReady:
      break;

    case ProfileInitStage::kFirstRun:
      break;

    case ProfileInitStage::kChoiceScreen:
      break;

    case ProfileInitStage::kNormalUI:
      // Stop forcing the portrait orientation once the normal UI is presented.
      _scopedForceOrientation.reset();
      break;

    case ProfileInitStage::kFinal:
      break;
  }
}

#pragma mark Private methods

- (void)attachProfileAgents {
  // TODO(crbug.com/355142171): Remove the DiscoverFeedProfileAgent?
  [_state addAgent:[[DiscoverFeedProfileAgent alloc] init]];

  [_state addAgent:[[ProfileActivityProfileAgent alloc] init]];
  [_state addAgent:[[PostRestoreProfileAgent alloc] init]];
  [_state addAgent:[[SearchEngineChoiceProfileAgent alloc] init]];
}

@end
