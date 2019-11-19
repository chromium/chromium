// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/content_capture/content_capture_manager.h"

#include "third_party/blink/renderer/core/content_capture/sent_nodes.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"

namespace blink {

ContentCaptureManager::ContentCaptureManager(LocalFrame& local_frame_root)
    : local_frame_root_(&local_frame_root) {
  DCHECK(local_frame_root.IsLocalRoot());
  sent_nodes_ = MakeGarbageCollected<SentNodes>();
  task_session_ = MakeGarbageCollected<TaskSession>(*sent_nodes_);
}

ContentCaptureManager::~ContentCaptureManager() = default;

DOMNodeId ContentCaptureManager::GetNodeId(Node& node) {
  if (first_node_holder_created_) {
    ScheduleTask(ContentCaptureTask::ScheduleReason::kContentChange);
  } else {
    ScheduleTask(ContentCaptureTask::ScheduleReason::kFirstContentChange);
    first_node_holder_created_ = true;
  }
  return DOMNodeIds::IdForNode(&node);
}

void ContentCaptureManager::ScheduleTask(
    ContentCaptureTask::ScheduleReason reason) {
  if (!content_capture_idle_task_.get()) {
    content_capture_idle_task_ = CreateContentCaptureTask();
  }
  content_capture_idle_task_->Schedule(reason);
}

scoped_refptr<ContentCaptureTask>
ContentCaptureManager::CreateContentCaptureTask() {
  return base::MakeRefCounted<ContentCaptureTask>(*local_frame_root_,
                                                  *task_session_);
}

void ContentCaptureManager::NotifyNodeDetached(const Node& node) {
  task_session_->OnNodeDetached(node);
}

void ContentCaptureManager::OnLayoutTextWillBeDestroyed(const Node& node) {
  NotifyNodeDetached(node);
  ScheduleTask(ContentCaptureTask::ScheduleReason::kContentChange);
}

void ContentCaptureManager::OnScrollPositionChanged() {
  ScheduleTask(ContentCaptureTask::ScheduleReason::kScrolling);
}

void ContentCaptureManager::OnNodeTextChanged(Node& node) {
  task_session_->OnNodeChanged(node);
  ScheduleTask(ContentCaptureTask::ScheduleReason::kContentChange);
}

void ContentCaptureManager::Trace(Visitor* visitor) {
  visitor->Trace(local_frame_root_);
  visitor->Trace(task_session_);
  visitor->Trace(sent_nodes_);
}

void ContentCaptureManager::Shutdown() {
  if (content_capture_idle_task_) {
    content_capture_idle_task_->Shutdown();
    content_capture_idle_task_.reset();
  }
}

}  // namespace blink
