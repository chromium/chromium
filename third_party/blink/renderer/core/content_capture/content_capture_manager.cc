// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/content_capture/content_capture_manager.h"

#include "base/time/time.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"

namespace blink {

namespace {

static constexpr base::TimeDelta kUserActivationExpiryPeriod = base::Seconds(5);

}  // namespace

ContentCaptureManager::UserActivation::UserActivation(
    const LocalFrame& local_frame)
    : local_frame(&local_frame), activation_time(base::TimeTicks::Now()) {}

void ContentCaptureManager::UserActivation::Trace(Visitor* visitor) const {
  visitor->Trace(local_frame);
}

ContentCaptureManager::ContentCaptureManager(LocalFrame& local_frame_root)
    : local_frame_root_(&local_frame_root) {
  DCHECK(local_frame_root.IsLocalRoot());
  task_session_ = MakeGarbageCollected<TaskSession>();
}

ContentCaptureManager::~ContentCaptureManager() = default;

void ContentCaptureManager::ScheduleTaskIfNeeded(const Node& node) {
  if (!task_session_)
    return;
  if (first_node_holder_created_) {
    ScheduleTask(
        UserActivated(node)
            ? ContentCaptureTask::ScheduleReason::kUserActivatedContentChange
            : ContentCaptureTask::ScheduleReason::
                  kNonUserActivatedContentChange);
  } else {
    ScheduleTask(ContentCaptureTask::ScheduleReason::kFirstContentChange);
    first_node_holder_created_ = true;
  }
}

bool ContentCaptureManager::UserActivated(const Node& node) const {
  if (auto* frame = node.GetDocument().GetFrame()) {
    return latest_user_activation_ &&
           latest_user_activation_->local_frame == frame &&
           (base::TimeTicks::Now() - latest_user_activation_->activation_time <
            kUserActivationExpiryPeriod);
  }
  return false;
}

void ContentCaptureManager::ScheduleTask(
    ContentCaptureTask::ScheduleReason reason) {
  DCHECK(task_session_);
  if (!content_capture_idle_task_) {
    content_capture_idle_task_ = CreateContentCaptureTask();
  }
  content_capture_idle_task_->Schedule(reason);
}

ContentCaptureTask* ContentCaptureManager::CreateContentCaptureTask() {
  return MakeGarbageCollected<ContentCaptureTask>(*local_frame_root_,
                                                  *task_session_);
}

void ContentCaptureManager::OnLayoutTextWillBeDestroyed(const Node& node) {
  if (!task_session_)
    return;
  task_session_->OnNodeDetached(node);
  ScheduleTask(
      UserActivated(node)
          ? ContentCaptureTask::ScheduleReason::kUserActivatedContentChange
          : ContentCaptureTask::ScheduleReason::kNonUserActivatedContentChange);
}

void ContentCaptureManager::OnScrollPositionChanged() {
  if (!task_session_)
    return;
  ScheduleTask(ContentCaptureTask::ScheduleReason::kScrolling);
}

void ContentCaptureManager::NotifyInputEvent(WebInputEvent::Type type,
                                             const LocalFrame& local_frame) {
  // Ignores events that are not actively interacting with the page. The ignored
  // input is the same as PaintTimeDetector::NotifyInputEvent().
  if (type == WebInputEvent::Type::kMouseMove ||
      type == WebInputEvent::Type::kMouseEnter ||
      type == WebInputEvent::Type::kMouseLeave ||
      type == WebInputEvent::Type::kKeyUp ||
      WebInputEvent::IsPinchGestureEventType(type)) {
    return;
  }

  latest_user_activation_ = MakeGarbageCollected<UserActivation>(local_frame);
}

void ContentCaptureManager::OnNodeTextChanged(Node& node) {
  if (!task_session_)
    return;
  task_session_->OnNodeChanged(node);
  ScheduleTask(
      UserActivated(node)
          ? ContentCaptureTask::ScheduleReason::kUserActivatedContentChange
          : ContentCaptureTask::ScheduleReason::kNonUserActivatedContentChange);
}

void ContentCaptureManager::Trace(Visitor* visitor) const {
  visitor->Trace(content_capture_idle_task_);
  visitor->Trace(local_frame_root_);
  visitor->Trace(task_session_);
  visitor->Trace(latest_user_activation_);
}

void ContentCaptureManager::OnFrameWasShown() {
  if (task_session_)
    return;
  task_session_ = MakeGarbageCollected<TaskSession>();
  ScheduleTask(ContentCaptureTask::ScheduleReason::kFirstContentChange);
}

void ContentCaptureManager::OnFrameWasHidden() {
  Shutdown();
}

void ContentCaptureManager::Shutdown() {
  if (content_capture_idle_task_) {
    content_capture_idle_task_->Shutdown();
    content_capture_idle_task_ = nullptr;
  }
  task_session_ = nullptr;
}

}  // namespace blink
