// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/core/content_capture/content_capture_task_histogram_reporter.h"

namespace blink {

ContentCaptureTaskHistogramReporter::ContentCaptureTaskHistogramReporter()
    : capture_content_time_histogram_(kCaptureContentTime, 0, 50000, 50) {}

ContentCaptureTaskHistogramReporter::~ContentCaptureTaskHistogramReporter() =
    default;

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

void ContentCaptureTaskHistogramReporter::RecordsSentContentCountPerDocument(
    int sent_content_count) {
  base::UmaHistogramCounts10000(kSentContentCount, sent_content_count);
}

}  // namespace blink
