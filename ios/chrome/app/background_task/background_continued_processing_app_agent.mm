// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_task/background_continued_processing_app_agent.h"

#import <BackgroundTasks/BackgroundTasks.h>

#import "base/apple/foundation_util.h"
#import "base/atomic_sequence_num.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "base/types/expected.h"
#import "ios/chrome/app/application_delegate/app_init_stage.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "ios/chrome/app/background_task/background_continued_processing_metrics.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_configuration.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_provider.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_request.h"
#import "ios/chrome/app/background_task/features.h"

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
namespace {

// Prefix for continued processing task identifiers. Must be kept in sync with
// `BackgroundContinuedProcessing+Info.plist`.
NSString* const kTaskIdentifierPrefix = @"continuedProcessingTask";

// Global sequence generator for unique background task registration
// identifiers.
base::AtomicSequenceNumber g_next_task_sequence_number;

// Returns a full task identifier composed of the main bundle identifier,
// prefix, client task identifier, and a unique sequence number.
NSString* FullTaskIdentifierForIdentifier(NSString* task_identifier) {
  NSString* bundle_identifier = NSBundle.mainBundle.bundleIdentifier;
  return [NSString stringWithFormat:@"%@.%@.%@.%d", bundle_identifier,
                                    kTaskIdentifierPrefix, task_identifier,
                                    g_next_task_sequence_number.GetNext()];
}

// Returns an equivalent `BGContinuedProcessingTaskRequestSubmissionStrategy`
// from a `BackgroundContinuedProcessingSubmissionStrategy`.
BGContinuedProcessingTaskRequestSubmissionStrategy
ConvertToBGContinuedProcessingTaskRequestSubmissionStrategy(
    BackgroundContinuedProcessingSubmissionStrategy strategy)
    API_AVAILABLE(ios(26.0)) {
  switch (strategy) {
    case BackgroundContinuedProcessingSubmissionStrategy::kQueue:
      return BGContinuedProcessingTaskRequestSubmissionStrategyQueue;
    case BackgroundContinuedProcessingSubmissionStrategy::kFail:
      return BGContinuedProcessingTaskRequestSubmissionStrategyFail;
  }
}

// Maps a `BGTaskScheduler` submission `error` to the corresponding request
// result.
BackgroundContinuedProcessingTaskRequestResult RequestResultFromSubmissionError(
    NSError* error) {
  if (!error || ![error.domain isEqualToString:BGTaskSchedulerErrorDomain]) {
    return BackgroundContinuedProcessingTaskRequestResult::
        kUnknownSubmissionError;
  }

  switch (static_cast<BGTaskSchedulerErrorCode>(error.code)) {
    case BGTaskSchedulerErrorCodeUnavailable:
      return BackgroundContinuedProcessingTaskRequestResult::
          kBGTaskSchedulerErrorUnavailable;
    case BGTaskSchedulerErrorCodeTooManyPendingTaskRequests:
      return BackgroundContinuedProcessingTaskRequestResult::
          kBGTaskSchedulerErrorTooManyPendingTaskRequests;
    case BGTaskSchedulerErrorCodeNotPermitted:
      return BackgroundContinuedProcessingTaskRequestResult::
          kBGTaskSchedulerErrorNotPermitted;
    case BGTaskSchedulerErrorCodeImmediateRunIneligible:
      return BackgroundContinuedProcessingTaskRequestResult::
          kBGTaskSchedulerErrorImmediateRunIneligible;
  }

  return BackgroundContinuedProcessingTaskRequestResult::
      kUnknownSubmissionError;
}

}  // namespace
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)

@interface BackgroundContinuedProcessingAppAgent ()

// YES if app initialization has finished and background continued processing is
// enabled. Checking `AppInitStage::kFinal` first ensures `base::FeatureList` is
// initialized before querying feature flags.
@property(nonatomic, readonly, getter=isAvailable) BOOL available;

@end

@implementation BackgroundContinuedProcessingAppAgent {
  // Active task contexts retained by the agent for their execution lifetime.
  NSMutableDictionary<NSString*, BackgroundContinuedProcessingTaskContext*>*
      _activeTasks;

  // Registered task providers, held weakly.
  CRBProtocolObservers<BackgroundContinuedProcessingTaskProvider>*
      _taskProviders;

  // Ensures calls are made on the main thread.
  SEQUENCE_CHECKER(_sequenceChecker);
}

#pragma mark - Initializer

- (instancetype)init {
  if ((self = [super init])) {
    _activeTasks = [[NSMutableDictionary alloc] init];
    _taskProviders = static_cast<CRBProtocolObservers<
        BackgroundContinuedProcessingTaskProvider>*>([CRBProtocolObservers
        observersWithProtocol:@protocol(
                                  BackgroundContinuedProcessingTaskProvider)]);
  }
  return self;
}

- (void)dealloc {
  NSArray<BackgroundContinuedProcessingTaskContext*>* activeTasks =
      [_activeTasks.allValues copy];
  for (BackgroundContinuedProcessingTaskContext* context in activeTasks) {
    [context setTaskCompletedWithSuccess:NO];
  }
  [_activeTasks removeAllObjects];
}

#pragma mark - Public

- (void)addTaskProvider:
    (id<BackgroundContinuedProcessingTaskProvider>)provider {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [_taskProviders addObserver:provider];
}

- (void)removeTaskProvider:
    (id<BackgroundContinuedProcessingTaskProvider>)provider {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [_taskProviders removeObserver:provider];
}

- (BackgroundContinuedProcessingTaskContext*)
    requestTaskWithIdentifier:(NSString*)identifier
                configuration:(BackgroundContinuedProcessingTaskConfiguration*)
                                  configuration {
  if (!self.available) {
    return nil;
  }

  return [self submitTaskRequestWithIdentifier:identifier
                                 configuration:configuration]
      .value_or(nil);
}

#pragma mark - SceneObservingAppAgent

- (void)appDidEnterBackground {
  [super appDidEnterBackground];
  if (!self.available) {
    return;
  }

  [_taskProviders backgroundProcessingBecameAvailable];
  __weak BackgroundContinuedProcessingAppAgent* weakSelf = self;
  [_taskProviders executeOnObservers:^(id provider) {
    [weakSelf requestTasksFromProvider:provider];
  }];
}

- (void)appDidEnterForeground {
  [super appDidEnterForeground];
  if (!self.available) {
    return;
  }

  [_taskProviders backgroundProcessingBecameUnnecessary];
}

#pragma mark - Private

- (BOOL)isAvailable {
  return self.appState.initStage == AppInitStage::kFinal &&
         IsBackgroundContinuedProcessingEnabled();
}

// Requests the tasks of `provider`, records the outcome of each request and
// passes the task context to each request that was successfully started.
- (void)requestTasksFromProvider:
    (id<BackgroundContinuedProcessingTaskProvider>)provider {
  for (BackgroundContinuedProcessingTaskRequest* request in
       [provider continuedProcessingTaskRequests]) {
    base::expected<BackgroundContinuedProcessingTaskContext*,
                   BackgroundContinuedProcessingTaskRequestResult>
        result = [self submitTaskRequestWithIdentifier:request.identifier
                                         configuration:request.configuration];

    base::UmaHistogramEnumeration(
        kBackgroundContinuedProcessingProviderRequestResultHistogram,
        result.error_or(
            BackgroundContinuedProcessingTaskRequestResult::kSubmitted));

    if (result.has_value() && request.startedHandler) {
      request.startedHandler(result.value());
    }
  }
}

// Submits a task request and returns its context, or the failure reason. Must
// only be called when background continued processing is available.
- (base::expected<BackgroundContinuedProcessingTaskContext*,
                  BackgroundContinuedProcessingTaskRequestResult>)
    submitTaskRequestWithIdentifier:(NSString*)identifier
                      configuration:
                          (BackgroundContinuedProcessingTaskConfiguration*)
                              configuration {
  CHECK(self.available);
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  CHECK(identifier.length > 0);
  CHECK(configuration);
  CHECK(configuration.title.length > 0);
  CHECK(configuration.subtitle);
  CHECK(configuration.expirationHandler);

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
  NSString* taskIdentifier = FullTaskIdentifierForIdentifier(identifier);

  __weak BackgroundContinuedProcessingAppAgent* weakSelf = self;
  if (@available(iOS 26.0, *)) {
    BOOL registered = [BGTaskScheduler.sharedScheduler
        registerForTaskWithIdentifier:taskIdentifier
                           usingQueue:dispatch_get_main_queue()
                        launchHandler:^(BGTask* task) {
                          // If the app agent was deallocated before the system
                          // invoked the launch handler, immediately fail the OS
                          // task to avoid leaking the background task.
                          if (!weakSelf) {
                            [task setTaskCompletedWithSuccess:NO];
                            return;
                          }
                          [weakSelf systemTriggeredExecutionForTask:task];
                        }];

    if (!registered) {
      DLOG(ERROR) << "Failed to register background task with identifier: "
                  << base::SysNSStringToUTF8(taskIdentifier);
      return base::unexpected(
          BackgroundContinuedProcessingTaskRequestResult::kRegistrationFailed);
    }

    BGContinuedProcessingTaskRequest* request =
        [[BGContinuedProcessingTaskRequest alloc]
            initWithIdentifier:taskIdentifier
                         title:configuration.title
                      subtitle:configuration.subtitle];
    request.strategy =
        ConvertToBGContinuedProcessingTaskRequestSubmissionStrategy(
            configuration.submissionStrategy);
    request.requiredResources = configuration.requiredResources;

    NSError* error = nil;
    BOOL submitted = [BGTaskScheduler.sharedScheduler submitTaskRequest:request
                                                                  error:&error];
    if (!submitted || error) {
      DLOG(ERROR) << "Failed to submit background task: "
                  << (error ? base::SysNSStringToUTF8(error.description)
                            : "unknown error");
      return base::unexpected(RequestResultFromSubmissionError(error));
    }

    BackgroundContinuedProcessingTaskContext* context =
        [[BackgroundContinuedProcessingTaskContext alloc]
            initWithTaskIdentifier:taskIdentifier
                     configuration:configuration
                     finishHandler:^{
                       [weakSelf taskDidFinishWithIdentifier:taskIdentifier];
                     }];
    _activeTasks[taskIdentifier] = context;
    return context;
  }
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)

  NOTREACHED();
}

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
// Handles task completion by removing the task identifier from `_activeTasks`.
- (void)taskDidFinishWithIdentifier:(NSString*)taskIdentifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [_activeTasks removeObjectForKey:taskIdentifier];
}

// Invoked by `BGTaskScheduler` when the operating system triggers execution
// for a registered continued processing task.
- (void)systemTriggeredExecutionForTask:(BGTask*)task {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  if (@available(iOS 26.0, *)) {
    BGContinuedProcessingTask* continuedTask =
        base::apple::ObjCCast<BGContinuedProcessingTask>(task);
    if (!continuedTask) {
      DLOG(ERROR) << "Received unexpected BGTask type: "
                  << base::SysNSStringToUTF8(NSStringFromClass([task class]));
      [task setTaskCompletedWithSuccess:NO];
      return;
    }

    BackgroundContinuedProcessingTaskContext* taskContext =
        _activeTasks[task.identifier];

    if (!taskContext) {
      DLOG(WARNING) << "Context was dropped before task launched: "
                    << base::SysNSStringToUTF8(task.identifier);
      [continuedTask setTaskCompletedWithSuccess:NO];
      return;
    }

    // Apple's `BGTaskScheduler` invokes the `launchHandler` directly on the
    // main queue specified during task registration.
    [taskContext attachUnderlyingTask:continuedTask];
  } else {
    [task setTaskCompletedWithSuccess:NO];
  }
}
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)

@end
