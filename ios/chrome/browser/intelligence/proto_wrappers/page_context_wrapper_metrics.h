// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_METRICS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_METRICS_H_

#import <Foundation/Foundation.h>

#import <string>

// PageContextWrapperMetrics's different PageContext tasks which can be tracked.
// LINT.IfChange(PageContextTaskVariants)
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
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/histograms.xml:PageContextTaskVariants)

// PageContextWrapperMetrics's different possible PageContext execution
// completion statuses.
// LINT.IfChange(PageContextStatusVariants)
enum class PageContextCompletionStatus {
  // Successfully generated PageContext.
  kSuccess,
  // An error occurred while generating PageContext.
  kFailure,
  // PageContext deemed the web page protected.
  kProtected,
  // PageContext extraction timed out.
  kTimeout,
  // PageContext is not extractable (e.g. unsupported MIME type or scheme).
  kNotExtractable,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/histograms.xml:PageContextStatusVariants)

// PageContextWrapperMetrics keeps track of the execution time of different
// PageContext tasks. It starts the timer of the `PageContextTask::kOverall`
// task at creation time.
@interface PageContextWrapperMetrics : NSObject

// Designated initializer with the apcConfigVariant.
- (instancetype)initWithAPCConfigVariant:(const std::string&)apcConfigVariant
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Execution started for `task`. Creates its associated timer with the current
// time as start time. This should not be called with
// `PageContextTask::kOverall`.
- (void)executionStartedForTask:(PageContextTask)task;

// Execution finished for `task`. Stops its associated timer with the current
// time as end time.
- (void)executionFinishedForTask:(PageContextTask)task
            withCompletionStatus:(PageContextCompletionStatus)completionStatus;

// Logs the byte size of the AnnotatedPageContent proto extracted.
- (void)logAnnotatedPageContentSize:(size_t)sizeInBytes;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_METRICS_H_
