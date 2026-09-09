// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_task/background_continued_processing_app_agent.h"

#import <BackgroundTasks/BackgroundTasks.h>

#import "base/apple/foundation_util.h"
#import "base/atomic_sequence_num.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/logging.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_configuration.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"
#import "ios/chrome/app/background_task/features.h"

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

}  // namespace

@implementation BackgroundContinuedProcessingAppAgent {
  // Active task contexts retained by the agent for their execution lifetime.
  NSMutableDictionary<NSString*, BackgroundContinuedProcessingTaskContext*>*
      _activeTasks;

  // Ensures calls are made on the main thread.
  SEQUENCE_CHECKER(_sequenceChecker);
}

#pragma mark - Initializer

- (instancetype)init {
  if ((self = [super init])) {
    _activeTasks = [[NSMutableDictionary alloc] init];
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

- (BackgroundContinuedProcessingTaskContext*)
    requestTaskWithIdentifier:(NSString*)identifier
                configuration:(BackgroundContinuedProcessingTaskConfiguration*)
                                  configuration {
  if (!IsBackgroundContinuedProcessingEnabled()) {
    return nil;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  CHECK(identifier.length > 0);
  CHECK(configuration);
  CHECK(configuration.title.length > 0);
  CHECK(configuration.expirationHandler);

  // Continued processing tasks must be initiated from an active foreground
  // scene.
  if (!self.appState.foregroundActiveScene) {
    DLOG(WARNING) << "Cannot request continued processing task without an "
                     "active foreground scene.";
    return nil;
  }

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
      return nil;
    }

    BGContinuedProcessingTaskRequest* request =
        [[BGContinuedProcessingTaskRequest alloc]
            initWithIdentifier:taskIdentifier
                         title:configuration.title
                      subtitle:configuration.subtitle];
    request.strategy = configuration.strategy;
    request.requiredResources = configuration.requiredResources;

    NSError* error = nil;
    BOOL submitted = [BGTaskScheduler.sharedScheduler submitTaskRequest:request
                                                                  error:&error];
    if (!submitted || error) {
      DLOG(ERROR) << "Failed to submit background task: "
                  << (error ? base::SysNSStringToUTF8(error.description)
                            : "unknown error");
      return nil;
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

  return nil;
}

#pragma mark - Private

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

@end
