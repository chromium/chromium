// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_METRICS_H_
#define IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_METRICS_H_

// Histogram recording the outcome of each continued processing task request
// made from a provider when the app enters the background.
inline constexpr char
    kBackgroundContinuedProcessingProviderRequestResultHistogram[] =
        "IOS.BackgroundContinuedProcessing.ProviderRequest.Result";

// Outcome of registering and submitting a continued processing task request
// with `BGTaskScheduler`, capturing whether the request was submitted or why it
// failed. Reported to UMA, so entries must not be renumbered or reused.
// LINT.IfChange(BackgroundContinuedProcessingTaskRequestResult)
enum class BackgroundContinuedProcessingTaskRequestResult {
  // `BGTaskScheduler` rejected the request without a known
  // `BGTaskSchedulerErrorCode`.
  kUnknownSubmissionError = 0,
  // The request was submitted to `BGTaskScheduler`.
  kSubmitted = 1,
  // `BGTaskScheduler` failed to register the task identifier.
  kRegistrationFailed = 2,
  // `BGTaskScheduler` rejected the request with
  // `BGTaskSchedulerErrorCodeUnavailable`.
  kBGTaskSchedulerErrorUnavailable = 3,
  // `BGTaskScheduler` rejected the request with
  // `BGTaskSchedulerErrorCodeTooManyPendingTaskRequests`.
  kBGTaskSchedulerErrorTooManyPendingTaskRequests = 4,
  // `BGTaskScheduler` rejected the request with
  // `BGTaskSchedulerErrorCodeNotPermitted`.
  kBGTaskSchedulerErrorNotPermitted = 5,
  // `BGTaskScheduler` rejected the request with
  // `BGTaskSchedulerErrorCodeImmediateRunIneligible`.
  kBGTaskSchedulerErrorImmediateRunIneligible = 6,
  kMaxValue = kBGTaskSchedulerErrorImmediateRunIneligible,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSBackgroundContinuedProcessingTaskRequestResult)

#endif  // IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_METRICS_H_
