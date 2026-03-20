// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_metrics.h"

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/strcat.h"
#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/metrics_constants.h"

namespace {

// Maps a `PageContextTask` to its corresponding histogram string constant.
const char* PageContextTaskToString(PageContextTask task) {
  switch (task) {
    case PageContextTask::kOverall:
      return kPageContextHistogramOverallTask;
    case PageContextTask::kScreenshot:
      return kPageContextHistogramScreenshotTask;
    case PageContextTask::kAnnotatedPageContent:
      return kPageContextHistogramAPCTask;
    case PageContextTask::kPDF:
      return kPageContextHistogramPDFTask;
    case PageContextTask::kInnerText:
      return kPageContextHistogramInnerTextTask;
  }
}

// Maps a `PageContextCompletionStatus` to its corresponding histogram string
// constant.
const char* PageContextCompletionStatusToString(
    PageContextCompletionStatus status) {
  switch (status) {
    case PageContextCompletionStatus::kSuccess:
      return kPageContextHistogramSuccessStatus;
    case PageContextCompletionStatus::kFailure:
      return kPageContextHistogramFailureStatus;
    case PageContextCompletionStatus::kProtected:
      return kPageContextHistogramPageProtectedStatus;
    case PageContextCompletionStatus::kTimeout:
      return kPageContextHistogramTimeoutStatus;
    case PageContextCompletionStatus::kNotExtractable:
      return kPageContextHistogramNotExtractableStatus;
  }
}

// Logs a constructed "IOS.PageContext.{Task}.{Status}.Latency" histogram for a
// PageContext task's execution latency.
void LogTaskExecutionLatency(PageContextTask task,
                             PageContextCompletionStatus status,
                             base::TimeDelta time_delta,
                             const std::string& apc_config_variant) {
  std::string histogram_name =
      base::StrCat({kPageContextHistogramPrefix, PageContextTaskToString(task),
                    PageContextCompletionStatusToString(status)});

  if (task == PageContextTask::kAnnotatedPageContent) {
    histogram_name = base::StrCat({histogram_name, apc_config_variant});
  }

  histogram_name =
      base::StrCat({histogram_name, kPageContextLatencyHistogramSuffix});

  base::UmaHistogramTimes(histogram_name.c_str(), time_delta);
}

}  // namespace

// Private TaskTimer class which holds a start time, end time and completion
// status. Its start time is set to its initialization time.
@interface TaskTimer : NSObject

// The start and end times in base::TimeTicks of the task.
@property(nonatomic, assign) base::TimeTicks startTime;
@property(nonatomic, assign) base::TimeTicks endTime;

// The completion status of the task.
@property(nonatomic, assign) PageContextCompletionStatus completionStatus;

@end

@implementation TaskTimer

- (instancetype)init {
  self = [super init];
  if (self) {
    _startTime = base::TimeTicks::Now();
  }
  return self;
}

@end

@implementation PageContextWrapperMetrics {
  // A map of PageContextTask (NSNumber*) -> TaskTimer.
  NSMutableDictionary<NSNumber*, TaskTimer*>* _taskTrackers;

  // The variant to inject into PageContext APC histograms based on the config.
  std::string _apcConfigVariant;
}

- (instancetype)initWithAPCConfigVariant:(const std::string&)apcConfigVariant {
  self = [super init];
  if (self) {
    _apcConfigVariant = apcConfigVariant;
    // Create the task tracker dictionary with one timer for the overall task.
    NSNumber* overallKey = @(static_cast<int>(PageContextTask::kOverall));
    _taskTrackers =
        [NSMutableDictionary dictionaryWithObject:[[TaskTimer alloc] init]
                                           forKey:overallKey];
  }
  return self;
}

- (void)executionStartedForTask:(PageContextTask)task {
  if (task == PageContextTask::kOverall) {
    NOTREACHED()
        << "The overall PageContext execution timer has already been started.";
  }

  NSNumber* key = @(static_cast<int>(task));
  CHECK(!_taskTrackers[key])
      << "The timer for this task has already been started.";
  _taskTrackers[key] = [[TaskTimer alloc] init];
}

- (void)executionFinishedForTask:(PageContextTask)task
            withCompletionStatus:(PageContextCompletionStatus)completionStatus {
  NSNumber* key = @(static_cast<int>(task));
  TaskTimer* timer = _taskTrackers[key];
  CHECK(timer) << "Timer for this task was never started.";
  timer.endTime = base::TimeTicks::Now();
  timer.completionStatus = completionStatus;

  if (task == PageContextTask::kOverall) {
    [self overallExecutionFinished];
  }
}

#pragma mark - Private

// Iterates over all tasks in the `_taskTrackers` map, and logs their execution
// latencies.
- (void)overallExecutionFinished {
  for (NSNumber* key in _taskTrackers) {
    TaskTimer* timer = _taskTrackers[key];

    // Only log metrics for tasks that have a valid end time.
    if (!timer.endTime.is_null()) {
      PageContextTask task = static_cast<PageContextTask>([key intValue]);
      PageContextCompletionStatus completionStatus = timer.completionStatus;
      base::TimeDelta latency = timer.endTime - timer.startTime;
      LogTaskExecutionLatency(task, completionStatus, latency,
                              _apcConfigVariant);
    }
  }
}

- (void)logAnnotatedPageContentSize:(size_t)sizeInBytes {
  base::UmaHistogramCounts10M(
      base::StrCat({kPageContextHistogramPrefix, kPageContextHistogramAPCTask,
                    _apcConfigVariant, kPageContextByteSizeHistogramSuffix}),
      sizeInBytes);
}

@end
