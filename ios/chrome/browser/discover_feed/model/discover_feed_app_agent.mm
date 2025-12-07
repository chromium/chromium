// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/discover_feed/model/discover_feed_app_agent.h"

#import <map>

#import "base/containers/contains.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_app_agent_profile_helper.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"

@implementation DiscoverFeedAppAgent {
  std::map<ProfileState*, DiscoverFeedAppAgentProfileHelper*> _helpers;
}

#pragma mark - ObservingAppAgent

- (void)appDidEnterBackground {
  if (IsAvoidFeedRefreshOnBackgroundEnabled()) {
    return;
  }

  for (const auto& [_, helper] : _helpers) {
    [helper refreshFeedInBackground];
  }
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    profileStateConnected:(ProfileState*)profileState {
  DCHECK(!base::Contains(_helpers, profileState));
  _helpers.insert(
      std::make_pair(profileState, [[DiscoverFeedAppAgentProfileHelper alloc]
                                       initWithProfileState:profileState]));
}

- (void)appState:(AppState*)appState
    profileStateDisconnected:(ProfileState*)profileState {
  auto iter = _helpers.find(profileState);
  DCHECK(iter != _helpers.end());
  [iter->second shutdown];
  _helpers.erase(iter);
}

@end
