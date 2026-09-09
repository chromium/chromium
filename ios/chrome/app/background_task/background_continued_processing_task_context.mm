// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"

#import <BackgroundTasks/BackgroundTasks.h>

#import <algorithm>

#import "base/check.h"
#import "base/debug/dump_without_crashing.h"
#import "base/logging.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_configuration.h"

namespace {

// Default total units of work/progress for a continued processing task.
constexpr int64_t kDefaultTotalUnits = 100;

}  // namespace

@implementation BackgroundContinuedProcessingTaskContext {
  // The formatted system task identifier.
  NSString* _taskIdentifier;

  // Title of the task.
  NSString* _title;

  // Subtitle of the task.
  NSString* _subtitle;

  // Callback invoked when the system expires the task.
  ProceduralBlock _expirationHandler;

  // The underlying `NSProgress` object tracking progress.
  NSProgress* _progress;

  // The underlying iOS background continued processing task provided by the OS.
  BGContinuedProcessingTask* _task API_AVAILABLE(ios(26.0));

  // Whether the task has concluded, either through completion, error, or
  // expiration.
  BOOL _completed;

  // The reported outcome of the task completion.
  BOOL _successfulCompletion;

  // Finish handler invoked upon task completion or expiration.
  ProceduralBlock _finishHandler;

  // Sequence checker for main thread.
  SEQUENCE_CHECKER(_sequenceChecker);
}

#pragma mark - Initializer

- (instancetype)initWithTaskIdentifier:(NSString*)taskIdentifier
                         configuration:
                             (BackgroundContinuedProcessingTaskConfiguration*)
                                 configuration
                         finishHandler:(ProceduralBlock)finishHandler {
  CHECK(taskIdentifier.length > 0);
  CHECK(configuration);
  CHECK(configuration.title.length > 0);
  CHECK(configuration.expirationHandler);

  if ((self = [super init])) {
    _taskIdentifier = [taskIdentifier copy];
    _title = [configuration.title copy];
    _subtitle = [configuration.subtitle copy];
    _expirationHandler = [configuration.expirationHandler copy];
    _finishHandler = [finishHandler copy];
    int64_t totalUnits = configuration.totalUnits > 0 ? configuration.totalUnits
                                                      : kDefaultTotalUnits;
    _progress = [NSProgress progressWithTotalUnitCount:totalUnits];
    _completed = NO;
    _successfulCompletion = NO;
  }
  return self;
}

#pragma mark - Public

- (NSString*)taskIdentifier {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _taskIdentifier;
}

- (double)fractionCompleted {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _progress.fractionCompleted;
}

- (BOOL)isCompleted {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _completed;
}

- (NSString*)title {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _title;
}

- (void)setTitle:(NSString*)title {
  [self updateTitle:title subtitle:_subtitle];
}

- (NSString*)subtitle {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _subtitle;
}

- (void)setSubtitle:(NSString*)subtitle {
  [self updateTitle:_title subtitle:subtitle];
}

- (void)updateTitle:(NSString*)title subtitle:(NSString*)subtitle {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  CHECK(title.length > 0);
  if (_completed) {
    return;
  }
  _title = [title copy];
  _subtitle = [subtitle copy];
  [self updateUnderlyingTaskTitleAndSubtitle];
}

- (void)incrementProgressByUnits:(int64_t)units {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_completed) {
    return;
  }
  [self setCompletedUnits:_progress.completedUnitCount + units];
}

- (void)setCompletedUnits:(int64_t)completedUnits {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_completed) {
    return;
  }
  int64_t clampedUnits =
      std::clamp<int64_t>(completedUnits, 0, _progress.totalUnitCount);
  _progress.completedUnitCount = clampedUnits;

  if (@available(iOS 26.0, *)) {
    if (_task) {
      _task.progress.completedUnitCount = clampedUnits;
    }
  }
}

- (void)setTaskCompletedWithSuccess:(BOOL)success {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_completed) {
    DLOG(WARNING) << "Attempted to complete already completed task: "
                  << base::SysNSStringToUTF8(_taskIdentifier);
    return;
  }

  _completed = YES;
  _successfulCompletion = success;
  _expirationHandler = nil;

  if (@available(iOS 26.0, *)) {
    if (_task) {
      _task.expirationHandler = nil;
      [_task setTaskCompletedWithSuccess:success];
      _task = nil;
    } else {
      [BGTaskScheduler.sharedScheduler
          cancelTaskRequestWithIdentifier:_taskIdentifier];
    }
  }

  ProceduralBlock finishHandler = _finishHandler;
  _finishHandler = nil;
  if (finishHandler) {
    finishHandler();
  }
}

#pragma mark - Internal

- (void)attachUnderlyingTask:(BGContinuedProcessingTask*)task
    API_AVAILABLE(ios(26.0)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  if (@available(iOS 26.0, *)) {
    if (_completed) {
      // If the task already finished before the OS delivered the underlying
      // task, immediately notify the system with the recorded outcome so the OS
      // does not keep an orphaned background execution budget alive.
      [task setTaskCompletedWithSuccess:_successfulCompletion];
      return;
    }

    _task = task;

    __weak BackgroundContinuedProcessingTaskContext* weakSelf = self;

    // Apple's `BGTaskScheduler` may invoke the expiration handler on a
    // background thread. Fast-path on the main thread, or hop to the main
    // thread if invoked off-thread (and make some noise).
    _task.expirationHandler = ^{
      if (NSThread.isMainThread) {
        [weakSelf handleSystemExpiration];
      } else {
        DLOG(WARNING) << "BGTask expirationHandler invoked off main thread.";
        base::debug::DumpWithoutCrashing();
        dispatch_async(dispatch_get_main_queue(), ^{
          [weakSelf handleSystemExpiration];
        });
      }
    };

    [self updateUnderlyingTaskTitleAndSubtitle];
    _task.progress.totalUnitCount = _progress.totalUnitCount;
    _task.progress.completedUnitCount = _progress.completedUnitCount;
  }
}

#pragma mark - Private

// Propagates the cached title and subtitle to the underlying system task.
- (void)updateUnderlyingTaskTitleAndSubtitle {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (@available(iOS 26.0, *)) {
    if (_task) {
      [_task updateTitle:_title subtitle:_subtitle ?: @""];
    }
  }
}

- (void)handleSystemExpiration {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_completed) {
    return;
  }

  _completed = YES;
  _successfulCompletion = NO;

  // Retain the handler in a local variable before nil-ing out the ivar to
  // avoid re-entrancy and retain issues during callback invocation.
  ProceduralBlock handler = _expirationHandler;
  _expirationHandler = nil;

  if (handler) {
    handler();
  }

  if (@available(iOS 26.0, *)) {
    if (_task) {
      _task.expirationHandler = nil;
      [_task setTaskCompletedWithSuccess:NO];
      _task = nil;
    }
  }

  ProceduralBlock finishHandler = _finishHandler;
  _finishHandler = nil;
  if (finishHandler) {
    finishHandler();
  }
}

- (void)dealloc {
  if (_completed) {
    return;
  }

  DLOG(WARNING) << "BackgroundContinuedProcessingTaskContext deallocated "
                   "without being marked as completed: "
                << base::SysNSStringToUTF8(_taskIdentifier);

  if (@available(iOS 26.0, *)) {
    if (_task) {
      _task.expirationHandler = nil;
      [_task setTaskCompletedWithSuccess:NO];
    } else {
      [BGTaskScheduler.sharedScheduler
          cancelTaskRequestWithIdentifier:_taskIdentifier];
    }
  }
}

@end
