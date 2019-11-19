// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_TASK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_TASK_H_

#include <memory>

#include "base/time/time.h"
#include "cc/paint/node_id.h"
#include "third_party/blink/renderer/core/content_capture/content_capture_task_histogram_reporter.h"
#include "third_party/blink/renderer/core/content_capture/task_session.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class WebContentCaptureClient;
class Document;
class LocalFrame;

// This class is used to capture the on-screen content and send them out
// through WebContentCaptureClient.
class CORE_EXPORT ContentCaptureTask : public RefCounted<ContentCaptureTask> {
  USING_FAST_MALLOC(ContentCaptureTask);

 public:
  enum class ScheduleReason {
    kFirstContentChange,
    kContentChange,
    kScrolling,
    kRetryTask,
  };

  enum class TaskState {
    kProcessRetryTask,
    kCaptureContent,
    kProcessCurrentSession,
    kStop,
  };

  ContentCaptureTask(LocalFrame& local_frame_root, TaskSession& task_session);
  virtual ~ContentCaptureTask();

  // Schedule the task if it hasn't been done.
  void Schedule(ScheduleReason reason);
  void Shutdown();

  // Make those const public for testing purpose.
  static constexpr size_t kBatchSize = 5;

  TaskState GetTaskStateForTesting() const { return task_state_; }

  void RunTaskForTestingUntil(TaskState stop_state) {
    task_stop_for_testing_ = stop_state;
    Run(nullptr);
  }

  void SetCapturedContentForTesting(
      const Vector<cc::NodeId>& captured_content) {
    captured_content_for_testing_ = captured_content;
  }

  void ClearDocumentSessionsForTesting();

  base::TimeDelta GetTaskNextFireIntervalForTesting() const;
  void CancelTaskForTesting();

 protected:
  // All protected data and methods are for testing purpose.
  // Return true if the task should pause.
  // TODO(michaelbai): Uses RunTaskForTestingUntil().
  virtual bool ShouldPause();
  virtual WebContentCaptureClient* GetWebContentCaptureClient(const Document&);

 private:
  // Callback method of delay_task_, runs the content capture task and
  // reschedule it if it necessary.
  void Run(TimerBase*);

  // The actual run method, return if the task completed.
  bool RunInternal();

  // Runs the sub task to capture content.
  bool CaptureContent();

  // Runs the sub task to process the captured content and the detached nodes.
  bool ProcessSession();

  // Processes |doc_session|, return True if |doc_session| has been processed,
  // otherwise, the process was interrupted because the task has to pause.
  bool ProcessDocumentSession(TaskSession::DocumentSession& doc_session);

  // Sends the captured content in batch.
  void SendContent(TaskSession::DocumentSession& doc_session);

  void ScheduleInternal(ScheduleReason reason);
  bool CaptureContent(Vector<cc::NodeId>& data);

  // Indicates if there is content change since last run.
  bool has_content_change_ = false;

  UntracedMember<LocalFrame> local_frame_root_;
  UntracedMember<TaskSession> task_session_;
  std::unique_ptr<TaskRunnerTimer<ContentCaptureTask>> delay_task_;
  TaskState task_state_ = TaskState::kStop;

  // Schedules the task with short delay for kFirstContentChange, kScrolling and
  // kRetryTask, with long delay for kContentChange.
  base::TimeDelta task_short_delay_;
  base::TimeDelta task_long_delay_;
  scoped_refptr<ContentCaptureTaskHistogramReporter> histogram_reporter_;
  base::Optional<TaskState> task_stop_for_testing_;
  base::Optional<Vector<cc::NodeId>> captured_content_for_testing_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CONTENT_CAPTURE_CONTENT_CAPTURE_TASK_H_
