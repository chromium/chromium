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

// Registers handler for the background refresh task. According to
// documentation, this must complete before the end of
// `applicationDidFinishLaunching`.
- (void)registerBackgroundRefreshTask {
  // TODO(crbug.com/1343695): Do not call this until Info.plist is modified.
  DCHECK(TRUE);
  __weak FeedAppAgent* weakSelf = self;
  [BGTaskScheduler.sharedScheduler
      registerForTaskWithIdentifier:kFeedBackgroundRefreshTaskIdentifier
                         usingQueue:nil
                      launchHandler:^(BGTask* task) {
                        [weakSelf handleBackgroundRefreshTask:task];
                      }];
}

// Schedules a background refresh task with an earliest begin date in the
// future. The OS limits to 1 refresh task at any time, and a new request will
// replace a previous request. Tasks are only executed in the background.
// TODO(crbug.com/1343695): It is critically important that we do not schedule
// other background fetch tasks (e.g., with other identifiers) anywhere,
// including other files. The OS only allows one fetch task at a time.
// Eventually, background fetches should be managed by a central manager.
- (void)scheduleBackgroundRefresh {
  // Do not DCHECK IsFeedBackgroundRefreshEnabled() because it is also called
  // from the background task handler, and the value could have changed during a
  // cold start.
  if (!IsFeedBackgroundRefreshEnabled() || ![self feedServiceIfAvailable]) {
    return;
  }
  BGAppRefreshTaskRequest* request = [[BGAppRefreshTaskRequest alloc]
      initWithIdentifier:kFeedBackgroundRefreshTaskIdentifier];
  request.earliestBeginDate =
      [self feedServiceIfAvailable]->GetEarliestBackgroundRefreshBeginDate();
  // Error in scheduling is intentionally not handled since the fallback is that
  // the user will just refresh in the foreground.
  // TODO(crbug.com/1343695): Consider logging error in histogram.
  [BGTaskScheduler.sharedScheduler submitTaskRequest:request error:nil];
}

// This method is called when the app is in the background.
- (void)handleBackgroundRefreshTask:(BGTask*)task {
  // Do not DCHECK IsFeedBackgroundRefreshEnabled() because the value could have
  // changed during a cold start.
  if (!IsFeedBackgroundRefreshEnabled() || ![self feedServiceIfAvailable]) {
    return;
  }
  if (IsRecurringBackgroundRefreshScheduleEnabled()) {
    [self scheduleBackgroundRefresh];
  }
  task.expirationHandler = ^{
    if ([self feedServiceIfAvailable]) {
      // There would be no refresh task to stop if there is no feed service
      // available.
      [self feedServiceIfAvailable]->HandleBackgroundRefreshTaskExpiration();
    }
  };
  [self feedServiceIfAvailable]->PerformBackgroundRefreshes(^(BOOL success) {
    [task setTaskCompletedWithSuccess:success];
  });
}

@end
