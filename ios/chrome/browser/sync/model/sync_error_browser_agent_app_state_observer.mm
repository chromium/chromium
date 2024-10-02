// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/sync_error_browser_agent_app_state_observer.h"

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/sync/model/sync_error_browser_agent.h"

@implementation SyncErrorBrowserAgentAppStateObserver {
  raw_ptr<SyncErrorBrowserAgent> _syncErrorBrowserAgent;
}

- (instancetype)initWithSyncErrorBrowserAgent:
    (SyncErrorBrowserAgent*)syncErrorBrowserAgent {
  if ((self = [super init])) {
    _syncErrorBrowserAgent = syncErrorBrowserAgent;
  }
  return self;
}

- (void)disconnect {
  _syncErrorBrowserAgent = nullptr;
}

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  if (appState.initStage != AppInitStage::kFinal) {
    return;
  }
  if (_syncErrorBrowserAgent) {
    _syncErrorBrowserAgent->AppStateDidUpdateToFinalStage();
  }
}

@end
