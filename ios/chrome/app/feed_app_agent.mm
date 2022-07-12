// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/feed_app_agent.h"

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/ntp/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FeedAppAgent

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    willTransitionToInitStage:(InitStage)nextInitStage {
  if (nextInitStage != InitStageFinal) {
    return;
  }

  if (IsWebChannelsEnabled()) {
    DiscoverFeedServiceFactory::GetForBrowserState(
        self.appState.mainBrowserState);
  }
}

@end
