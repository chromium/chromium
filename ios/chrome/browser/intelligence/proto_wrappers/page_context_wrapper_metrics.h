// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_METRICS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_METRICS_H_

#import <Foundation/Foundation.h>

// PageContextWrapperMetrics's different PageContext tasks which can be tracked.
enum class PageContextTask {
  // Overall PageContextWrapper execution.
  kOverall,
  // Screenshot retrieval task execution.
  kScreenshot,
  // AnnotatedPageContent (APC) retrieval task execution.
  kAnnotatedPageContent,
  // PDF retrieval task execution.
  kPDF,
  // innerText retrieval task execution.
  kInnerText,
};

// PageContextWrapperMetrics's different possible PageContext execution
// completion statuses.
enum class PageContextCompletionStatus {
  // Successfully generated PageContext.
  kSuccess,
  // An error occurred while generating PageContext.
  kFailure,
  // PageContext deemed the web page protected.
  kProtected,
  // PageContext extraction timed out.
  kTimeout,
};

// PageContextWrapperMetrics keeps track of the execution time of different
// PageContext tasks. It starts the timer of the `PageContextTask::kOverall`
// task at creation time.
@interface PageContextWrapperMetrics : NSObject

// Execution started for `task`. Creates its associated timer with the current
// time as start time. This should not be called with
// `PageContextTask::kOverall`.
- (void)executionStartedForTask:(PageContextTask)task;

// Execution finished for `task`. Stops its associated timer with the current
// time as end time.
- (void)executionFinishedForTask:(PageContextTask)task
            withCompletionStatus:(PageContextCompletionStatus)completionStatus;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_METRICS_H_
