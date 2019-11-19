// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/content_capture/content_capture_task.h"

#include "base/auto_reset.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/blink/public/web/web_content_capture_client.h"
#include "third_party/blink/public/web/web_content_holder.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

ContentCaptureTask::ContentCaptureTask(LocalFrame& local_frame_root,
                                       TaskSession& task_session)
    : local_frame_root_(&local_frame_root), task_session_(&task_session) {
  local_frame_root.Client()
      ->GetWebContentCaptureClient()
      ->GetTaskTimingParameters(task_short_delay_, task_long_delay_);
  // The histogram is all about time, just disable it if high resolution isn't
  // supported.
  if (base::TimeTicks::IsHighResolution()) {
    histogram_reporter_ =
        base::MakeRefCounted<ContentCaptureTaskHistogramReporter>();
    task_session_->SetSentNodeCountCallback(
        WTF::BindRepeating(&ContentCaptureTaskHistogramReporter::
                               RecordsSentContentCountPerDocument,
                           histogram_reporter_));
  }
}

ContentCaptureTask::~ContentCaptureTask() {}

void ContentCaptureTask::Shutdown() {
  DCHECK(local_frame_root_);
  local_frame_root_ = nullptr;
}

bool ContentCaptureTask::CaptureContent(Vector<cc::NodeId>& data) {
  if (captured_content_for_testing_) {
    data = captured_content_for_testing_.value();
    return true;
  }
  // Because this is called from a different task, the frame may be in any
  // lifecycle step so we need to early-out in many cases.
  if (const auto* root_frame_view = local_frame_root_->View()) {
    if (const auto* cc_layer = root_frame_view->RootCcLayer()) {
      if (auto* layer_tree_host = cc_layer->layer_tree_host()) {
        std::vector<cc::NodeId> content;
        if (layer_tree_host->CaptureContent(&content)) {
          for (auto c : content)
            data.push_back(std::move(c));
          return true;
        }
        return false;
      }
    }
  }
  return false;
}

bool ContentCaptureTask::CaptureContent() {
  DCHECK(task_session_);
  Vector<cc::NodeId> buffer;
  if (histogram_reporter_)
    histogram_reporter_->OnCaptureContentStarted();
  bool result = CaptureContent(buffer);
  if (histogram_reporter_)
    histogram_reporter_->OnCaptureContentEnded(buffer.size());
  if (!buffer.IsEmpty())
    task_session_->SetCapturedContent(buffer);
  return result;
}

void ContentCaptureTask::SendContent(
    TaskSession::DocumentSession& doc_session) {
  auto* document = doc_session.GetDocument();
  DCHECK(document);
  auto* client = GetWebContentCaptureClient(*document);
  DCHECK(client);

  if (histogram_reporter_)
    histogram_reporter_->OnSendContentStarted();
  WebVector<WebContentHolder> content_batch;
  content_batch.reserve(kBatchSize);
  // Only send changed content after the new content was sent.
  bool sending_changed_content = !doc_session.HasUnsentCapturedContent();
  while (content_batch.size() < kBatchSize) {
    Node* node;
    if (sending_changed_content)
      node = doc_session.GetNextChangedNode();
    else
      node = doc_session.GetNextUnsentNode();
    if (!node)
      break;
    content_batch.emplace_back(WebContentHolder(*node));
  }
  if (!content_batch.empty()) {
    if (sending_changed_content) {
      client->DidUpdateContent(content_batch);
    } else {
      client->DidCaptureContent(content_batch, !doc_session.FirstDataHasSent());
      doc_session.SetFirstDataHasSent();
    }
  }
  if (histogram_reporter_)
    histogram_reporter_->OnSendContentEnded(content_batch.size());
}

WebContentCaptureClient* ContentCaptureTask::GetWebContentCaptureClient(
    const Document& document) {
  if (auto* frame = document.GetFrame())
    return frame->Client()->GetWebContentCaptureClient();
  return nullptr;
}

bool ContentCaptureTask::ProcessSession() {
  DCHECK(task_session_);
  while (auto* document_session =
             task_session_->GetNextUnsentDocumentSession()) {
    if (!ProcessDocumentSession(*document_session))
      return false;
    if (ShouldPause())
      return !task_session_->HasUnsentData();
  }
  return true;
}

bool ContentCaptureTask::ProcessDocumentSession(
    TaskSession::DocumentSession& doc_session) {
  // If no client, we don't need to send it at all.
  auto* content_capture_client =
      GetWebContentCaptureClient(*doc_session.GetDocument());
  if (!content_capture_client) {
    doc_session.Reset();
    return true;
  }

  while (doc_session.HasUnsentCapturedContent() ||
         doc_session.HasUnsentChangedContent()) {
    SendContent(doc_session);
    if (ShouldPause()) {
      return !doc_session.HasUnsentData();
    }
  }
  // Sent the detached nodes.
  if (doc_session.HasUnsentDetachedNodes())
    content_capture_client->DidRemoveContent(doc_session.MoveDetachedNodes());
  DCHECK(!doc_session.HasUnsentData());
  return true;
}

bool ContentCaptureTask::RunInternal() {
  base::AutoReset<TaskState> state(&task_state_, TaskState::kProcessRetryTask);
  // Already shutdown.
  if (!local_frame_root_)
    return true;

  do {
    switch (task_state_) {
      case TaskState::kProcessRetryTask:
        if (task_session_->HasUnsentData()) {
          if (!ProcessSession())
            return false;
        }
        task_state_ = TaskState::kCaptureContent;
        break;
      case TaskState::kCaptureContent:
        if (!has_content_change_)
          return true;
        if (!CaptureContent()) {
          // Don't schedule task again in this case.
          return true;
        }
        has_content_change_ = false;
        if (!task_session_->HasUnsentData())
          return true;

        task_state_ = TaskState::kProcessCurrentSession;
        break;
      case TaskState::kProcessCurrentSession:
        return ProcessSession();
        break;
      default:
        return true;
    }
  } while (!ShouldPause());
  return false;
}

void ContentCaptureTask::Run(TimerBase*) {
  TRACE_EVENT0("content_capture", "RunTask");
  if (!RunInternal()) {
    ScheduleInternal(ScheduleReason::kRetryTask);
  }
}

void ContentCaptureTask::ScheduleInternal(ScheduleReason reason) {
  DCHECK(local_frame_root_);

  base::TimeDelta delay;
  switch (reason) {
    case ScheduleReason::kFirstContentChange:
    case ScheduleReason::kScrolling:
    case ScheduleReason::kRetryTask:
      delay = task_short_delay_;
      break;
    case ScheduleReason::kContentChange:
      delay = task_long_delay_;
      break;
  }

  // Return if the current task is about to run soon.
  if (delay_task_ && delay_task_->IsActive() &&
      delay_task_->NextFireInterval() < delay) {
    return;
  }

  if (!delay_task_) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        local_frame_root_->GetTaskRunner(TaskType::kInternalContentCapture);
    delay_task_ = std::make_unique<TaskRunnerTimer<ContentCaptureTask>>(
        task_runner, this, &ContentCaptureTask::Run);
  }

  if (delay_task_->IsActive())
    delay_task_->Stop();

  delay_task_->StartOneShot(delay, FROM_HERE);
  TRACE_EVENT_INSTANT1("content_capture", "ScheduleTask",
                       TRACE_EVENT_SCOPE_THREAD, "reason", reason);
}

void ContentCaptureTask::Schedule(ScheduleReason reason) {
  DCHECK(local_frame_root_);
  has_content_change_ = true;
  if (histogram_reporter_)
    histogram_reporter_->OnContentChanged();
  ScheduleInternal(reason);
}

bool ContentCaptureTask::ShouldPause() {
  if (task_stop_for_testing_) {
    return task_state_ == task_stop_for_testing_.value();
  }
  return ThreadScheduler::Current()->ShouldYieldForHighPriorityWork();
}

void ContentCaptureTask::ClearDocumentSessionsForTesting() {
  task_session_->ClearDocumentSessionsForTesting();
}

base::TimeDelta ContentCaptureTask::GetTaskNextFireIntervalForTesting() const {
  return delay_task_ && delay_task_->IsActive()
             ? delay_task_->NextFireInterval()
             : base::TimeDelta();
}

void ContentCaptureTask::CancelTaskForTesting() {
  if (delay_task_ && delay_task_->IsActive())
    delay_task_->Stop();
}

}  // namespace blink
