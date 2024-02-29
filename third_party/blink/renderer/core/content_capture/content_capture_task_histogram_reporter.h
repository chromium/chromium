// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_TASK_HISTOGRAM_REPORTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_TASK_HISTOGRAM_REPORTER_H_

#include <optional>

#include "base/time/time.h"
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
      "ContentCapture.CaptureContentTime2";
  static constexpr char kSendContentTime[] = "ContentCapture.SendContentTime";
  static constexpr char kSentContentCount[] =
      "ContentCapture.SentContentCount2";
  static constexpr char kTaskDelayInMs[] = "ContentCapture.TaskDelayTimeInMs";
  static constexpr char kTaskRunsPerCapture[] =
      "ContentCapture.TaskRunsPerCapture";

  ContentCaptureTaskHistogramReporter();
  ~ContentCaptureTaskHistogramReporter();

  void OnContentChanged();
  void OnTaskScheduled(bool record_task_delay);
  // Invoked on every task starts.
  void OnTaskRun();
  // Invoked on a capturing session starts, a session begins with
  // OnCaptureContentStarted(), ends with OnAllCaptureContentSent().
  void OnCaptureContentStarted();
  // Invoked on the on-screen content captured and ready to be sent out.
  void OnCaptureContentEnded(size_t captured_content_count);
  void OnSendContentStarted();
  void OnSendContentEnded(size_t sent_content_count);
  // Invoked on a capturing session ends, at that time, all captured changes
  // which include the new, changed and removed content has been sent.
  void OnAllCapturedContentSent();
  void RecordsSentContentCountPerDocument(int sent_content_count);

 private:
  void MayRecordTaskRunsPerCapture();

  // The time of first content change since the last content captured.
  std::optional<base::TimeTicks> content_change_time_;
  // The copy of |content_change_time| after the content has been captured; we
  // need to record the time the content has been sent, |content_change_time_|
  // shall be released for the next content change.
  std::optional<base::TimeTicks> captured_content_change_time_;
  // The time to start capturing content.
  base::TimeTicks capture_content_start_time_;
  // The time to start sending content.
  base::TimeTicks send_content_start_time_;
  // The time when the task is scheduled, is valid if kTaskDelayInMs needs to be
  // recorded.
  base::TimeTicks task_scheduled_time_;
  // Counts the task run times to complete a capture which includes capturing
  // and sending the content.
  int task_runs_per_capture_ = 0;

  // Records time to capture the content, its range is from 0 to 50,000
  // microseconds.
  CustomCountHistogram capture_content_time_histogram_;
  // Records time to send the content, its range is from 0 to 50,000
  // microseconds.
  CustomCountHistogram send_content_time_histogram_;
  // Records the number of times ContentCapture task run to complete a capture
  // which includes capturing and sending the content.
  CustomCountHistogram task_runs_per_capture_histogram_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_TASK_HISTOGRAM_REPORTER_H_
