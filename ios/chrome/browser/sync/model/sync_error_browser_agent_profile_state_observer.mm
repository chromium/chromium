// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/sync_error_browser_agent_profile_state_observer.h"

#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/sync/model/sync_error_browser_agent.h"

@interface SyncErrorBrowserAgentProfileStateObserver () <ProfileStateObserver>
@end

@implementation SyncErrorBrowserAgentProfileStateObserver {
  __weak ProfileState* _profileState;
  raw_ptr<SyncErrorBrowserAgent> _syncErrorBrowserAgent;
}

- (instancetype)initWithProfileState:(ProfileState*)profileState
               syncErrorBrowserAgent:(SyncErrorBrowserAgent*)browserAgent {
  if ((self = [super init])) {
    _syncErrorBrowserAgent = browserAgent;
    _profileState = profileState;
  }
  return self;
}

- (void)start {
  [_profileState addObserver:self];
}

- (void)disconnect {
  [_profileState removeObserver:self];
  _profileState = nil;

  _syncErrorBrowserAgent = nullptr;
}

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage == ProfileInitStage::kFinal) {
    if (_syncErrorBrowserAgent) {
      _syncErrorBrowserAgent->ProfileStateDidUpdateToFinalStage();
    }
  }
}

@end
