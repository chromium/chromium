// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/core/content_capture/content_capture_task_histogram_reporter.h"

namespace blink {

// static
constexpr char ContentCaptureTaskHistogramReporter::kCaptureContentTime[];
constexpr char ContentCaptureTaskHistogramReporter::kSendContentTime[];
constexpr char ContentCaptureTaskHistogramReporter::kSentContentCount[];
constexpr char ContentCaptureTaskHistogramReporter::kTaskDelayInMs[];
constexpr char ContentCaptureTaskHistogramReporter::kTaskRunsPerCapture[];

ContentCaptureTaskHistogramReporter::ContentCaptureTaskHistogramReporter()
    : capture_content_time_histogram_(kCaptureContentTime, 0, 50000, 50),
      send_content_time_histogram_(kSendContentTime, 0, 50000, 50),
      task_runs_per_capture_histogram_(kTaskRunsPerCapture, 0, 100, 50) {}

ContentCaptureTaskHistogramReporter::~ContentCaptureTaskHistogramReporter() =
    default;

void ContentCaptureTaskHistogramReporter::OnTaskScheduled(
    bool record_task_delay) {
  // Always save the latest schedule time.
  task_scheduled_time_ =
      record_task_delay ? base::TimeTicks::Now() : base::TimeTicks();
}

void ContentCaptureTaskHistogramReporter::OnTaskRun() {
  if (!task_scheduled_time_.is_null()) {
    base::UmaHistogramCustomTimes(
        kTaskDelayInMs, base::TimeTicks::Now() - task_scheduled_time_,
        base::Milliseconds(1), base::Seconds(128), 100);
  }
  task_runs_per_capture_++;
}

void ContentCaptureTaskHistogramReporter::OnCaptureContentStarted() {
  capture_content_start_time_ = base::TimeTicks::Now();
}

void ContentCaptureTaskHistogramReporter::OnCaptureContentEnded(
    size_t captured_content_count) {
  if (!captured_content_count) {
    return;
  }
  base::TimeDelta delta = base::TimeTicks::Now() - capture_content_start_time_;
  capture_content_time_histogram_.CountMicroseconds(delta);
}

void ContentCaptureTaskHistogramReporter::OnSendContentStarted() {
  send_content_start_time_ = base::TimeTicks::Now();
}

void ContentCaptureTaskHistogramReporter::OnSendContentEnded(
    size_t sent_content_count) {
  if (!sent_content_count) {
    return;
  }
  send_content_time_histogram_.CountMicroseconds(base::TimeTicks::Now() -
                                                 send_content_start_time_);
}

void ContentCaptureTaskHistogramReporter::OnAllCapturedContentSent() {
  task_runs_per_capture_histogram_.Count(task_runs_per_capture_);
  task_runs_per_capture_ = 0;
}

void ContentCaptureTaskHistogramReporter::RecordsSentContentCountPerDocument(
    int sent_content_count) {
  base::UmaHistogramCounts10000(kSentContentCount, sent_content_count);
}

}  // namespace blink
