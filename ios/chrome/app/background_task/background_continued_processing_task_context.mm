// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"

#import <BackgroundTasks/BackgroundTasks.h>

#import <algorithm>
#import <cmath>

#import "base/check.h"
#import "base/check_op.h"
#import "base/debug/dump_without_crashing.h"
#import "base/logging.h"
#import "base/sequence_checker.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_configuration.h"

namespace {

// Fraction of `totalUnits` reached upon completing the linear progress
// phase of the stepped incremental progress.
constexpr double kLinearProgressRatio = 0.70;

// Fraction of `totalUnits` approached asymptotically during the
// asymptotic progress phase of the stepped incremental progress.
constexpr double kAsymptoticProgressRatio = 0.98;

// Divisor applied during the asymptotic progress phase of the stepped
// incremental progress to compute the step size toward the asymptotic ceiling.
constexpr int64_t kAsymptoticProgressDivisor = 25;

// Returns the clamped asymptotic progress ceiling units for a task with
// `totalUnits`. The ceiling approaches `kAsymptoticProgressRatio` of
// `totalUnits`, clamped to [0, totalUnits - 1] to guarantee that stepped
// incremental progress never prematurely reaches 100% completion before
// the task finishes.
int64_t ClampedAsymptoticProgressCeilingUnits(int64_t totalUnits) {
  return std::max<int64_t>(
      0, std::min<int64_t>(totalUnits - 1,
                           static_cast<int64_t>(std::round(
                               totalUnits * kAsymptoticProgressRatio))));
}

// Returns the clamped linear progress ceiling units for a task with
// `totalUnits`. The ceiling approaches `kLinearProgressRatio` of
// `totalUnits`, clamped to
// [0, ClampedAsymptoticProgressCeilingUnits(totalUnits)] to guarantee that the
// linear phase ceiling never exceeds the asymptotic ceiling.
int64_t ClampedLinearProgressCeilingUnits(int64_t totalUnits) {
  return std::min<int64_t>(
      ClampedAsymptoticProgressCeilingUnits(totalUnits),
      static_cast<int64_t>(std::round(totalUnits * kLinearProgressRatio)));
}

// Returns the expected progress units for a given `stepCount` in the linear
// progress phase of the discrete steps algorithm.
int64_t LinearUnitsForStep(int64_t stepCount,
                           double stepRatio,
                           int64_t linearCeiling) {
  return std::min<int64_t>(
      linearCeiling, static_cast<int64_t>(std::round(stepCount * stepRatio)));
}

// Returns the effective step count corresponding to `units` in the linear
// progress phase of the discrete steps algorithm.
int64_t LinearStepForUnits(int64_t units, double stepRatio) {
  return static_cast<int64_t>(std::round(units / stepRatio));
}

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

  // Finish handler invoked upon task completion or expiration.
  ProceduralBlock _finishHandler;

  // The underlying iOS background continued processing task provided by the OS.
  BGContinuedProcessingTask* _task API_AVAILABLE(ios(26.0));

  // The underlying `NSProgress` object tracking progress units.
  NSProgress* _progress;

  // Expected number of progress steps in the linear progress phase.
  int64_t _expectedStepCount;

  // Whether the task has concluded, either through completion, error, or
  // expiration.
  BOOL _completed;

  // The reported outcome of the task completion.
  BOOL _successfulCompletion;

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
  CHECK_GT(configuration.totalUnits, 0);
  CHECK_GT(configuration.expectedStepCount, 0);

  if ((self = [super init])) {
    _taskIdentifier = [taskIdentifier copy];
    _title = [configuration.title copy];
    _subtitle = [configuration.subtitle copy];
    _expirationHandler = [configuration.expirationHandler copy];
    _finishHandler = [finishHandler copy];
    _progress =
        [NSProgress progressWithTotalUnitCount:configuration.totalUnits];
    _expectedStepCount = configuration.expectedStepCount;
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

- (double)fractionCompleted {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _progress.fractionCompleted;
}

- (int64_t)completedUnits {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _progress.completedUnitCount;
}

- (int64_t)totalUnits {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _progress.totalUnitCount;
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

- (void)incrementProgressByUnits:(int64_t)units {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  CHECK_GE(units, 0);
  if (_completed) {
    return;
  }
  [self setCompletedUnits:_progress.completedUnitCount + units];
}

// TODO(crbug.com/561662582): Refactor step progress algorithm and related
// properties to separate file.
- (void)incrementStepProgress {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_completed) {
    return;
  }

  const int64_t currentUnits = _progress.completedUnitCount;
  const int64_t totalUnits = _progress.totalUnitCount;
  const int64_t linearCeiling = ClampedLinearProgressCeilingUnits(totalUnits);
  const int64_t asymptoticCeiling =
      ClampedAsymptoticProgressCeilingUnits(totalUnits);

  int64_t newUnits = currentUnits;

  if (currentUnits < linearCeiling) {
    // When below the linear ceiling, progress advances linearly toward
    // `linearCeiling`.
    const double stepRatio =
        static_cast<double>(linearCeiling) / _expectedStepCount;
    const int64_t currentStep = LinearStepForUnits(currentUnits, stepRatio);
    const int64_t nextStep = currentStep + 1;
    const int64_t computedUnits =
        LinearUnitsForStep(nextStep, stepRatio, linearCeiling);
    // Ensure positive progress (+1 unit minimum) even in low-resolution edge
    // cases where `stepRatio` < 1.0.
    newUnits = std::min<int64_t>(linearCeiling,
                                 std::max(currentUnits + 1, computedUnits));
  } else if (currentUnits < asymptoticCeiling) {
    // When the linear ceiling has been reached, progress approaches
    // `asymptoticCeiling` asymptotically by advancing a fractional step of the
    // remaining units available.
    const int64_t remaining = asymptoticCeiling - currentUnits;
    const int64_t step =
        std::max<int64_t>(1, remaining / kAsymptoticProgressDivisor);
    newUnits = std::min<int64_t>(asymptoticCeiling, currentUnits + step);
  }

  // Ensure progress never decreases.
  newUnits = std::max(currentUnits, newUnits);

  [self setCompletedUnits:newUnits];
}

- (BOOL)isCompleted {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _completed;
}

- (void)setTaskCompletedWithSuccess:(BOOL)success {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_completed) {
    DLOG(WARNING) << "Attempted to complete already completed task: "
                  << base::SysNSStringToUTF8(_taskIdentifier);
    return;
  }

  if (success) {
    [self setCompletedUnits:_progress.totalUnitCount];
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
