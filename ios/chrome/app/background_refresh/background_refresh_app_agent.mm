// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_refresh/background_refresh_app_agent.h"

#import <BackgroundTasks/BackgroundTasks.h>

#import "base/ios/block_types.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/background_refresh/app_refresh_provider.h"
#import "ios/chrome/app/background_refresh/background_refresh_metrics.h"
#import "ios/chrome/app/background_refresh_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

// Debug NSUserDefaults key used to collect debug data.
NSString* triggeredBackgroundRefreshesKey =
    @"debug_number_of_triggered_background_refreshes";

// Debug NSUserDefaults key used to collect debug data.
// Number of times systemTriggeredRefreshForTask was run when appState was
// UIApplicationStateActive.
NSString* appStateActiveCountDuringBackgroundRefreshKey =
    @"debug_app_state_active_count_during_background_refresh";

// Debug NSUserDefaults key used to collect debug data.
// Number of times systemTriggeredRefreshForTask was run when appState was
// UIApplicationStateInactive.
NSString* appStateInactiveCountDuringBackgroundRefreshKey =
    @"debug_app_state_inactive_count_during_background_refresh";

// Debug NSUserDefaults key used to collect debug data.
// Number of times systemTriggeredRefreshForTask was run when appState was
// UIApplicationStateBackground.
NSString* appStateBackgroundCountDuringBackgroundRefreshKey =
    @"debug_app_state_background_count_during_background_refresh";

// Debug NSUserDefaults key used to collect debug data.
// Number of times systemTriggeredRefreshForTask was run with no due tasks.
NSString* appStateDueCountDuringBackgroundRefreshKey =
    @"debug_app_state_no_due_count_during_background_refresh";

}  // namespace

@interface BGTaskScheduler (cheating)
- (void)_simulateLaunchForTaskWithIdentifier:(NSString*)ident;
@end

@interface BackgroundRefreshAppAgent ()
@property(nonatomic) NSMutableSet<AppRefreshProvider*>* providers;
@end

@implementation BackgroundRefreshAppAgent

- (instancetype)init {
  if ((self = [super init])) {
    _providers = [NSMutableSet set];
    [self registerBackgroundRefreshTask];
  }
  return self;
}

- (void)addAppRefreshProvider:(AppRefreshProvider*)provider {
  CHECK(provider);
  [self.providers addObject:provider];
}

#pragma mark - SceneObservingAppAgent

- (void)appDidEnterBackground {
  [self requestAppRefreshWithDelay:30 * 60.0];  // 30 minutes.
}

#pragma mark - Private

- (void)registerBackgroundRefreshTask {
  auto handler = ^(BGTask* task) {
    [self systemTriggeredRefreshForTask:task];
  };

  // TODO(crbug.com/354919106):  Consider moving this task to a queue known to
  // Chromium, so it's easy to safely thread hop.
  [BGTaskScheduler.sharedScheduler
      registerForTaskWithIdentifier:kAppBackgroundRefreshTaskIdentifier
                         usingQueue:nil
                      launchHandler:handler];
}

// Debugging note: To induce the scheduler to call this task, you should
//   (1) Set a breakpoint sometime after `-registerBaskgroundRefreshTask` is
//       called.
//   (2) When the app is paused, run the following command in the debugger:
//         e -l objc -- (void)[[BGTaskScheduler sharedScheduler]
//         _simulateLaunchForTaskWithIdentifier:@"chrome.app.refresh"]
//   (3) Resume execution.
//
// Note that calling -requestAppRefreshWithDelay: with a delay value of 0.0
// will call _simulateLaunchForTaskWithIdentifier: immediately.
//
// To trigger the expiration handler (that is, to forcibly expire the task):
//   (1) Set a breakpoint in the followsing method, after the for-loop that
//       calls all of the providers.
//   (2) Make sure this method is called by triggering the task as described
//       above.
//   (3) When the app is paused, run the following command in the debugger:
//         e -l objc -- (void)[[BGTaskScheduler sharedScheduler]
//         _simulateExpirationForTaskWithIdentifier:@"chrome.app.refresh"]
//   (4) Resume execution.
//
//   Remember also that BACKGROUND REFRESH REQUIRES A DEVICE. It doesn't work
//   on simulators at all.

// Handle background refresh. This is called by the (OS) background task
// scheduler and is **not called on the main thread**.
- (void)systemTriggeredRefreshForTask:(BGTask*)task {
  // TODO(crbug.com/354919106): This is the simplest possible implementation,
  // and it provides no thread safety or signalling to the app state about
  // status. Some of the many things that must be implemented for this to work
  // correctly:
  //  - No processing if this is a safe mode launch.
  //  - Configure an expiration handler on `task`, which cancels all refresh
  //    tasks.
  //  - Update the app state for both starting and ending refresh work; this
  //    must hapopen on the main thread, and further processing should wait on
  //    it.
  //  - Handle tracking completion of each task, and only signal success if
  //    all tasks succeeded overall.

  // TODO(crbug.com/354918794): Remove this code once not needed anymore.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  int triggeredRefreshCount =
      [defaults integerForKey:triggeredBackgroundRefreshesKey];
  [defaults setInteger:triggeredRefreshCount + 1
                forKey:triggeredBackgroundRefreshesKey];

  // TODO(crbug.com/354918794): Remove this code once not needed anymore.
  // ApplicationState must be called from the main thread.
  dispatch_async(dispatch_get_main_queue(), ^{
    NSString* appState;
    switch ([[UIApplication sharedApplication] applicationState]) {
      case UIApplicationStateActive:
        appState = appStateActiveCountDuringBackgroundRefreshKey;
        break;
      case UIApplicationStateInactive:
        appState = appStateInactiveCountDuringBackgroundRefreshKey;
        break;
      case UIApplicationStateBackground:
        appState = appStateBackgroundCountDuringBackgroundRefreshKey;
        break;
    }
    [defaults setInteger:[defaults integerForKey:appState] + 1 forKey:appState];
  });

  ProceduralBlock completion = ^{
    [task setTaskCompletedWithSuccess:YES];
  };

  // YES if there is at least one due task.
  BOOL hasDueTasks = NO;
  for (AppRefreshProvider* provider in self.providers) {
    // Only execute due tasks.
    if ([provider isDue]) {
      hasDueTasks = YES;
      [provider handleRefreshWithCompletion:completion];
    }
  }

  // TODO(crbug.com/354918794): Remove this code once not needed anymore.
  if (!hasDueTasks) {
    [defaults
        setInteger:[defaults integerForKey:
                                 appStateDueCountDuringBackgroundRefreshKey] +
                   1
            forKey:appStateDueCountDuringBackgroundRefreshKey];
  }
}

// Request that app refresh runs no sooner than `delay` seconds from now.
// Multiple requests for refresh will be coalesced.
// TODO(crbug.com/354918222): Derive `delay` from the refresh intervals of the
// providers.
- (void)requestAppRefreshWithDelay:(NSTimeInterval)delay {
  // Schedule requests only if flag is enabled.
  if (!IsAppBackgroundRefreshEnabled()) {
    return;
  }

  // TODO(crbug.com/354918222): coalesce multiple requests so there's only ever
  // a single scheduled refresh pending.
  BGAppRefreshTaskRequest* request = [[BGAppRefreshTaskRequest alloc]
      initWithIdentifier:kAppBackgroundRefreshTaskIdentifier];
  request.earliestBeginDate = [NSDate dateWithTimeIntervalSinceNow:delay];
  NSError* error = nil;
  [BGTaskScheduler.sharedScheduler submitTaskRequest:request error:&error];
  BGTaskSchedulerErrorActions action = BGTaskSchedulerErrorActions::kUnknown;
  if (error) {
    BGTaskSchedulerErrorCode code = (BGTaskSchedulerErrorCode)error.code;
    switch (code) {
      case BGTaskSchedulerErrorCodeUnavailable:
        action = BGTaskSchedulerErrorActions::kErrorCodeUnavailable;
        break;
      case BGTaskSchedulerErrorCodeNotPermitted:
        action = BGTaskSchedulerErrorActions::kErrorCodeNotPermitted;
        break;
      case BGTaskSchedulerErrorCodeTooManyPendingTaskRequests:
        action =
            BGTaskSchedulerErrorActions::kErrorCodeTooManyPendingTaskRequests;
        break;
    }
  } else {
    action = BGTaskSchedulerErrorActions::kSuccess;
  }

  base::UmaHistogramEnumeration(kBGTaskSchedulerErrorHistogram, action);

  // Time-saving debug mode.
  if (delay == 0.0) {
    [[BGTaskScheduler sharedScheduler] _simulateLaunchForTaskWithIdentifier:
                                           kAppBackgroundRefreshTaskIdentifier];
  }
}

@end
