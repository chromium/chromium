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
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

// Debug NSUserDefaults key used to reset debug data. If the stored value of
// this key is less than `resetDataValue`, then all of the foillowing debug
// data counts are reset to 0.
NSString* resetDebugDataKey = @"debug_number_of_triggered_background_refreshes";
NSInteger resetDebugDataValue = 1;

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
NSString* noTasksDueCountDuringBackgroundRefreshKey =
    @"debug_no_tasks_due_count_during_background_refresh";

// Debug NSUserDefaults key used to collect debug data.
// Number of times systemTriggeredRefreshForTask was with the last startup not
// being clean (as defined by ApplicationContext::WasLastShutdownClean());
NSString* dirtyShutdownDuringAppRefreshKey =
    @"debug_dirty_shutdown_during_app_refresh";

}  // namespace

@interface BGTaskScheduler (cheating)
- (void)_simulateLaunchForTaskWithIdentifier:(NSString*)ident;
@end

@interface BackgroundRefreshAppAgent ()
// The providers registered.
@property(nonatomic) NSMutableSet<AppRefreshProvider*>* providers;
// The subset of `providers` that are currently running refresh tasks.
@property(nonatomic) NSMutableSet<AppRefreshProvider*>* activeProviders;
@end

// General note on threading: the IOS task scheduler invokes the configured
// blocks on a non-main thread. This class's primary job is to route background
// refresh work to the specific classes (instances of AppRefreshProvider) which
// actually perform the refresh tasks. In order for this to happen on Chromium
// threads, all of the task dispatch and management work is done on the main
// thread. The methods that handle that work are thus main-sequence affine, and
// are guarded by a sequence checker. The configured methods that handle task
// execution and cancellation, which are called directly by the iOS scheduler on
// a non-main thread, must therefore only consist of dispacthing to a
// corresponding main-thread method.
//
// For clarity, the methods expected to be only called from the task scheduler
// on a non-main thread have names beginning with `systemTriggered`. The
// corresponding main-thread methods have names beginning with `handle`.

@implementation BackgroundRefreshAppAgent {
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)init {
  if ((self = [super init])) {
    _providers = [NSMutableSet set];
    _activeProviders = [NSMutableSet set];
    [self registerBackgroundRefreshTask];
  }
  return self;
}

- (void)addAppRefreshProvider:(AppRefreshProvider*)provider {
  CHECK(provider);
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.providers addObject:provider];
}

#pragma mark - SceneObservingAppAgent

- (void)appDidEnterForeground {
  // Log if the last session was cleanly shutdown.
  if (!GetApplicationContext()->WasLastShutdownClean()) {
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    [defaults
        setInteger:[defaults integerForKey:dirtyShutdownDuringAppRefreshKey] + 1
            forKey:dirtyShutdownDuringAppRefreshKey];
  }
}

- (void)appDidEnterBackground {
  [self requestAppRefreshWithDelay:30 * 60.0];  // 30 minutes.
}

#pragma mark - Private

- (void)registerBackgroundRefreshTask {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  // TODO(crbug.com/354918794): Remove debug logging once not needed anymore.
  [self maybeResetDebugData];

  auto handler = ^(BGTask* task) {
    [self systemTriggeredExecutionForTask:task];
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
- (void)systemTriggeredExecutionForTask:(BGTask*)task {
  // TODO(crbug.com/354919106): This is still an incomplete implementation. Some
  // of the things that must be implemented for this to work correctly:
  //  - No processing if this is a safe mode launch.
  //  - Update the app state for both starting and ending refresh work; this
  //    must hapopen on the main thread, and further processing should wait on
  //    it. There are hooks (-refreshStarted and -refreshComplete) for this but
  //    they are no-ops.

  __weak __typeof(self) weakSelf = self;
  __weak __typeof(task) weakTask = task;
  task.expirationHandler = ^{
    [weakSelf systemTriggeredExpirationForTask:weakTask];
  };

  // TODO(crbug.com/354918794): Remove this code once not needed anymore.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  int triggeredRefreshCount =
      [defaults integerForKey:triggeredBackgroundRefreshesKey];
  [defaults setInteger:triggeredRefreshCount + 1
                forKey:triggeredBackgroundRefreshesKey];

  // Hop on to the main thread for task execution.
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf handleExecutionForTask:task];
  });
}

// Handle task expiration. This is called by the (OS) background task scheduler
// shortly before the task expires; it is **not called on the main thread**.
- (void)systemTriggeredExpirationForTask:(BGTask*)task {
  // Hop to main thread to cancel tasks.
  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf handleExpiratonForTask:task];
  });
}

// Handles `task` execution on the main thread.
- (void)handleExecutionForTask:(BGTask*)task {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  // TODO(crbug.com/354918794): Remove this code once not needed anymore.
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
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setInteger:[defaults integerForKey:appState] + 1 forKey:appState];

  [self refreshStarted];
  for (AppRefreshProvider* provider in self.providers) {
    // Only execute due tasks.
    if ([provider isDue]) {
      // Track running providers. The completion handler will remove tasks as
      // they complete.
      [self.activeProviders addObject:provider];

      __weak __typeof(self) weakSelf = self;
      ProceduralBlock completion = ^{
        [weakSelf handleCompletedProvider:provider forTask:task];
      };
      [provider handleRefreshWithCompletion:completion];
    }
  }
  // TODO(crbug.com/354918794): Remove this code once not needed anymore.
  if (self.activeProviders.count == 0) {
    [defaults
        setInteger:
            [defaults integerForKey:noTasksDueCountDuringBackgroundRefreshKey] +
            1
            forKey:noTasksDueCountDuringBackgroundRefreshKey];
  }
}

// Handle `task` running out of execution time.
- (void)handleExpiratonForTask:(BGTask*)task {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // TODO(crbug.com/354919106): While this should correctly handle task
  // expiration, there's no mechanism for looging or informing the app as a
  // whole that this has happened.

  // Cancel all provider tasks. The completion callback will not be called.
  for (AppRefreshProvider* provider in self.activeProviders) {
    [provider cancelRefresh];
  }
  // Stop tracking all remaining providers.
  [self.activeProviders removeAllObjects];
  // Signal that the refresh is complete.
  [self refreshComplete];
}

// Handle `provider` completing its work for `task`. If all active providers are
// complete, mark `task` as complete.
- (void)handleCompletedProvider:(AppRefreshProvider*)provider
                        forTask:(BGTask*)task {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.activeProviders removeObject:provider];
  if (self.activeProviders.count == 0) {
    [task setTaskCompletedWithSuccess:YES];
    [self refreshComplete];
  }
}

- (void)refreshStarted {
  // TODO(crbug.com/354919106): At a minimum, this should signal to the app
  // state that refresh has started.
  // Better would be tell the app state to call back when the right init stage
  // has been reached, which might be immediate. There's currently no control
  // flow to do this.
  //
  // The design intent for this class is that it is agnostic to whether the app
  // is actually foregrounded or not.
}

- (void)refreshComplete {
  // TODO(crbug.com/354919106): Signal to the app state that background refresh
  // is done. This should, if the app is not yet in the foreground, cause the
  // app to enter a state where background termination is not considered a
  // crash.
  //
  // The design intent for this class is that it is agnostic to whether the app
  // is actually foregrounded or not. The app state will care about how to
  // handle -refreshComplete in the foreground vs the background, but this class
  // will not.
}

// Request that app refresh runs no sooner than `delay` seconds from now.
// Multiple requests for refresh will be coalesced.
// TODO(crbug.com/354918222): Derive `delay` from the refresh intervals of the
// providers.
- (void)requestAppRefreshWithDelay:(NSTimeInterval)delay {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
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

// TODO(crbug.com/354918794): Remove this method once not needed anymore.
- (void)maybeResetDebugData {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSInteger resetValue = [defaults integerForKey:resetDebugDataKey];
  if (resetValue < resetDebugDataValue) {
    NSArray* debugKeys = @[
      triggeredBackgroundRefreshesKey,
      appStateActiveCountDuringBackgroundRefreshKey,
      appStateInactiveCountDuringBackgroundRefreshKey,
      appStateBackgroundCountDuringBackgroundRefreshKey,
      noTasksDueCountDuringBackgroundRefreshKey,
      dirtyShutdownDuringAppRefreshKey
    ];
    for (NSString* key in debugKeys) {
      [defaults setInteger:0 forKey:key];
    }
    [defaults setInteger:resetDebugDataValue forKey:resetDebugDataKey];
  }
}

@end
