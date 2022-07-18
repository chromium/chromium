// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/feed_app_agent.h"

#import <BackgroundTasks/BackgroundTasks.h>

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/discover_feed/feed_constants.h"
#import "ios/chrome/browser/ntp/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FeedAppAgent

- (void)registerBackgroundRefreshTask {
  __weak FeedAppAgent* weakSelf = self;
  [BGTaskScheduler.sharedScheduler
      registerForTaskWithIdentifier:kFeedBackgroundRefreshTaskIdentifier
                         usingQueue:nil
                      launchHandler:^(BGTask* task) {
                        [weakSelf handleBackgroundRefreshTask:task];
                      }];
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    willTransitionToInitStage:(InitStage)nextInitStage {
  if (nextInitStage != InitStageFinal) {
    return;
  }
  // Starting the DiscoverFeedService at app initialization is required for the
  // FollowingFeed.
  if (IsWebChannelsEnabled() && self.appState.mainBrowserState) {
    DiscoverFeedServiceFactory::GetForBrowserState(
        self.appState.mainBrowserState);
  }
}

#pragma mark - SceneObservingAppAgent

- (void)appDidEnterBackground {
  if (IsFeedBackgroundRefreshEnabled() && [self feedServiceIfAvailable]) {
    [self scheduleBackgroundRefresh];
  }
}

- (void)appDidEnterForeground {
  if (IsFeedBackgroundRefreshEnabled()) {
    // This is not strictly necessary, but it makes it more explicit. The OS
    // limits to 1 refresh task at any time, and a new request will replace a
    // previous request. Tasks are only executed in the background.
    // TODO(crbug.com/1344866): Coordinate background tasks when more are added.
    [BGTaskScheduler.sharedScheduler cancelAllTaskRequests];
  }
}

#pragma mark - Helpers

// Returns the DiscoverFeedService if it has already been created.
- (DiscoverFeedService*)feedServiceIfAvailable {
  if (self.appState.mainBrowserState) {
    // TODO(crbug.com/1343695): Do not cause the service to be created if has
    // not already been created. Return nil if it has not already been created.
  }
  return nil;
}

// Schedules a background refresh task with an earliest begin date in the
// future. The OS limits to 1 refresh task at any time, and a new request will
// replace a previous request. Tasks are only executed in the background.
// TODO(crbug.com/1343695): It is critically important that we do not schedule
// other background fetch tasks (e.g., with other identifiers) anywhere,
// including other files. The OS only allows one fetch task at a time.
// Eventually, background fetches should be managed by a central manager.
- (void)scheduleBackgroundRefresh {
  DCHECK(IsFeedBackgroundRefreshEnabled());
  if (![self feedServiceIfAvailable]) {
    return;
  }
  BGAppRefreshTaskRequest* request = [[BGAppRefreshTaskRequest alloc]
      initWithIdentifier:kFeedBackgroundRefreshTaskIdentifier];
  // TODO(crbug.com/1343695): Use DiscoverFeedService to set the earliest begin
  // date on the request. This ensures that any task scheduling from anywhere
  // sets the correct earliest begin date. Error in scheduling is intentionally
  // not handled since the fallback is that the user will just refresh in the
  // foreground.
  [BGTaskScheduler.sharedScheduler submitTaskRequest:request error:nil];
}

// This method is called when the app is in the background.
- (void)handleBackgroundRefreshTask:(BGTask*)task {
  DCHECK(IsFeedBackgroundRefreshEnabled());
  if (![self feedServiceIfAvailable]) {
    return;
  }
  // TODO(crbug.com/1343695): Use DiscoverFeedService to refresh the feed. Then
  // mark the task completed or failed. Also set a task expiration handler.
  [task setTaskCompletedWithSuccess:NO];
}

@end
