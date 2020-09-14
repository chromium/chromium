// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_TASK_HISTOGRAM_REPORTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_TASK_HISTOGRAM_REPORTER_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

// This class collects and reports metric data for the ContentCaptureTask.
class CORE_EXPORT ContentCaptureTaskHistogramReporter
    : public RefCounted<ContentCaptureTaskHistogramReporter> {
 public:
  // Visible for testing.
  static constexpr char kCaptureContentDelayTime[] =
      "ContentCapture.CaptureContentDelayTime";
  static constexpr char kCaptureContentTime[] =
      "ContentCapture.CaptureContentTime";
  static constexpr char kSendContentTime[] = "ContentCapture.SendContentTime";
  static constexpr char kSentContentCount[] = "ContentCapture.SentContentCount";
  static constexpr char kTaskDelayInMs[] = "ContentCapture.TaskDelayTimeInMs";

  ContentCaptureTaskHistogramReporter();
  ~ContentCaptureTaskHistogramReporter();

  void OnContentChanged();
  void OnTaskScheduled(bool record_task_delay);
  void OnTaskRun();
  void OnCaptureContentStarted();
  void OnCaptureContentEnded(size_t captured_content_count);
  void OnSendContentStarted();
  void OnSendContentEnded(size_t sent_content_count);
  void RecordsSentContentCountPerDocument(size_t sent_content_count);

 private:
  // The time of first content change since the last content captured.
  base::Optional<base::TimeTicks> content_change_time_;
  // The copy of |content_change_time| after the content has been captured; we
  // need to record the time the content has been sent, |content_change_time_|
  // shall be released for the next content change.
  base::Optional<base::TimeTicks> captured_content_change_time_;
  // The time to start capturing content.
  base::TimeTicks capture_content_start_time_;
  // The time to start sending content.
  base::TimeTicks send_content_start_time_;
  // The time when the task is scheduled, is valid if kTaskDelayInMs needs to be
  // recorded.
  base::TimeTicks task_scheduled_time_;

  // Records time from first content change to content that has been sent, its
  // range is 500ms from to 30s.
  CustomCountHistogram capture_content_delay_time_histogram_;
  // Records time to capture the content, its range is from 0 to 50,000
  // microseconds.
  CustomCountHistogram capture_content_time_histogram_;
  // Records time to send the content, its range is from 0 to 50,000
  // microseconds.
  CustomCountHistogram send_content_time_histogram_;
  // Records total count has been sent, its range is from 0 to 10,000.
  LinearHistogram sent_content_count_histogram_;
  // Records time taken for the task to start after it is schedule, its range is
  // 1ms to 128s. The time of task that was scheduled for the retry wasn't
  // measured because it is always 500ms.
  CustomCountHistogram task_delay_time_in_ms_histogram_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_TASK_HISTOGRAM_REPORTER_H_
