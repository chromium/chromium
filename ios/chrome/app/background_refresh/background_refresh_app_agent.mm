// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_refresh/background_refresh_app_agent.h"

#import <BackgroundTasks/BackgroundTasks.h>

#import "base/ios/block_types.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/app/application_delegate/app_init_stage.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/background_refresh/app_refresh_provider.h"
#import "ios/chrome/app/background_refresh/background_refresh_app_agent_audience.h"
#import "ios/chrome/app/background_refresh/background_refresh_metrics.h"
#import "ios/chrome/app/background_refresh_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"

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
  BGTask* _pendingTask;
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

- (void)appDidEnterBackground {
  [self requestAppRefresh];
}

- (void)appState:(AppState*)appState
    willTransitionToInitStage:(AppInitStage)nextInitStage {
  if (nextInitStage > AppInitStage::kBrowserObjectsForBackgroundHandlers &&
      _pendingTask) {
    [self executeProvidersForTask:_pendingTask];
  }
}

#pragma mark - Private

- (void)registerBackgroundRefreshTask {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

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
// uncomment the debug code at the end of -requestAppRefresh, or:
//   (1) Set a breakpoint sometime after `-registerBaskgroundRefreshTask` is
//       called.
//   (2) When the app is paused, run the following command in the debugger:
//         e -l objc -- (void)[[BGTaskScheduler sharedScheduler]
//         _simulateLaunchForTaskWithIdentifier:@"chrome.app.refresh"]
//   (3) Resume execution.
//
// To trigger the expiration handler (that is, to forcibly expire the task):
//   (1) Set a breakpoint in -handleExecutionForTask:, after the for-loop that
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
  [task setExpirationHandler:^{
    [weakSelf systemTriggeredExpirationForTask:weakTask];
  }];

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

// Main-threrad handler for task execution.
// Records metrics for backgroud refresh invocation.
// If the app is ready, triggers provider execution.
// If not, caches the task for later executuion.
- (void)handleExecutionForTask:(BGTask*)task {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  // Record cold/warm start for this refresh.
  LaunchTypeForBackgroundRefreshActions launchType =
      LaunchTypeForBackgroundRefreshActions::kUnknown;
  BOOL continueExecution = YES;
  if (self.appState.initStage <=
      AppInitStage::kBrowserObjectsForBackgroundHandlers) {
    // Early launch, no threads available! Record it and don't continue.
    launchType =
        LaunchTypeForBackgroundRefreshActions::kLaunchTypePreBrowserObjects;
    continueExecution = NO;
  } else if (self.startupInformation.isColdStart) {
    launchType = LaunchTypeForBackgroundRefreshActions::kLaunchTypeCold;
  } else {
    launchType = LaunchTypeForBackgroundRefreshActions::kLaunchTypeWarm;
  }
  base::UmaHistogramEnumeration(kLaunchTypeForBackgroundRefreshHistogram,
                                launchType);

  // Schedule another refresh.
  [self requestAppRefresh];

  // If it's possible to handle the tasks now, do it. If not, mark the task as
  // pending.
  if (continueExecution) {
    [self executeProvidersForTask:task];
  } else {
    _pendingTask = task;
  }
}

- (void)executeProvidersForTask:(BGTask*)task {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  // Remove any pending task.
  _pendingTask = nil;

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

  // If none of the providers were due, mark the refresh complete.
  if (self.activeProviders.count == 0) {
    [task setTaskCompletedWithSuccess:YES];
    [self refreshComplete];
  }
}

// Handle `task` running out of execution time.
- (void)handleExpiratonForTask:(BGTask*)task {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // TODO(crbug.com/354919106): While this should correctly handle task
  // expiration, there's no mechanism for looging or informing the app as a
  // whole that this has happened.

  // Remove any pending task.
  _pendingTask = nil;

  // Cancel all provider tasks. The completion callback will not be called.
  for (AppRefreshProvider* provider in self.activeProviders) {
    [provider cancelRefresh];
  }
  // Stop tracking all remaining providers.
  [self.activeProviders removeAllObjects];
  // Mark the task unsuccessful.
  [task setTaskCompletedWithSuccess:NO];
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.audience backgroundRefreshDidStart];
}

- (void)refreshComplete {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.audience backgroundRefreshDidEnd];
}

// Request that app refresh runs no sooner than `delay` seconds from now.
// Multiple requests for refresh will be coalesced.
- (void)requestAppRefresh {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // Schedule requests only if flag is enabled.
  if (!IsAppBackgroundRefreshEnabled()) {
    return;
  }

  // Find the minimum refresh interval.
  base::TimeDelta delay = base::TimeDelta::Max();
  for (AppRefreshProvider* provider in self.providers) {
    delay = std::min(delay, provider.refreshInterval);
  }
  if (delay == base::TimeDelta::Max()) {
    // No provider provided a usable refresh interval.
    return;
  }
  NSTimeInterval delayInSeconds = delay.InSecondsF();

  // TODO(crbug.com/354918222): coalesce multiple requests so there's only ever
  // a single scheduled refresh pending.
  BGAppRefreshTaskRequest* request = [[BGAppRefreshTaskRequest alloc]
      initWithIdentifier:kAppBackgroundRefreshTaskIdentifier];
  request.earliestBeginDate =
      [NSDate dateWithTimeIntervalSinceNow:delayInSeconds];
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

  // Time-saving debug mode; uncomment this to immediately trigger the refresh
  // task. Please do not delete the commented code!
  // [[BGTaskScheduler sharedScheduler] _simulateLaunchForTaskWithIdentifier:
  //                                      kAppBackgroundRefreshTaskIdentifier];
}

@end
