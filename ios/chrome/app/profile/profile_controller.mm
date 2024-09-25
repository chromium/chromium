// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/profile_controller.h"

#import "ios/chrome/app/profile/post_restore_profile_agent.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_profile_agent.h"
#import "ios/chrome/browser/profile_metrics/model/profile_activity_profile_agent.h"

@interface ProfileController () <ProfileStateObserver>
@end

@implementation ProfileController

- (instancetype)initWithAppState:(AppState*)appState {
  if ((self = [super init])) {
    _state = [[ProfileState alloc] initWithAppState:appState];
    [_state addObserver:self];
  }
  return self;
}

#pragma mark ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  switch (nextInitStage) {
    case ProfileInitStage::InitStageLoadProfile:
      break;

    case ProfileInitStage::InitStageProfileLoaded:
      [self attachProfileAgents];
      break;

    case ProfileInitStage::InitStageEnterprise:
      break;

    case ProfileInitStage::InitStagePrepareUI:
      break;

    case ProfileInitStage::InitStageUIReady:
      break;

    case ProfileInitStage::InitStageFirstRun:
      break;

    case ProfileInitStage::InitStageChoiceScreen:
      break;

    case ProfileInitStage::InitStageNormalUI:
      break;

    case ProfileInitStage::InitStageFinal:
      break;
  }
}

#pragma mark Private methods

- (void)attachProfileAgents {
  // TODO(crbug.com/355142171): Remove the DiscoverFeedProfileAgent?
  [_state addAgent:[[DiscoverFeedProfileAgent alloc] init]];

  [_state addAgent:[[ProfileActivityProfileAgent alloc] init]];
  [_state addAgent:[[PostRestoreProfileAgent alloc] init]];
}

@end
