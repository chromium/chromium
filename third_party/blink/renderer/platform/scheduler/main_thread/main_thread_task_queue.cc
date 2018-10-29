// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;

namespace internal {
using base::sequence_manager::internal::TaskQueueImpl;
}

// static
const char* MainThreadTaskQueue::NameForQueueType(
    MainThreadTaskQueue::QueueType queue_type) {
  switch (queue_type) {
    case MainThreadTaskQueue::QueueType::kControl:
      return "control_tq";
    case MainThreadTaskQueue::QueueType::kDefault:
      return "default_tq";
    case MainThreadTaskQueue::QueueType::kUnthrottled:
      return "unthrottled_tq";
    case MainThreadTaskQueue::QueueType::kFrameLoading:
      return "frame_loading_tq";
    case MainThreadTaskQueue::QueueType::kFrameThrottleable:
      return "frame_throttleable_tq";
    case MainThreadTaskQueue::QueueType::kFrameDeferrable:
      return "frame_deferrable_tq";
    case MainThreadTaskQueue::QueueType::kFramePausable:
      return "frame_pausable_tq";
    case MainThreadTaskQueue::QueueType::kFrameUnpausable:
      return "frame_unpausable_tq";
    case MainThreadTaskQueue::QueueType::kCompositor:
      return "compositor_tq";
    case MainThreadTaskQueue::QueueType::kIdle:
      return "idle_tq";
    case MainThreadTaskQueue::QueueType::kTest:
      return "test_tq";
    case MainThreadTaskQueue::QueueType::kFrameLoadingControl:
      return "frame_loading_control_tq";
    case MainThreadTaskQueue::QueueType::kV8:
      return "v8_tq";
    case MainThreadTaskQueue::QueueType::kIPC:
      return "ipc_tq";
    case MainThreadTaskQueue::QueueType::kInput:
      return "input_tq";
    case MainThreadTaskQueue::QueueType::kDetached:
      return "detached_tq";
    case MainThreadTaskQueue::QueueType::kCleanup:
      return "cleanup_tq";
    case MainThreadTaskQueue::QueueType::kOther:
      return "other_tq";
    case MainThreadTaskQueue::QueueType::kWebSchedulingUserInteraction:
      return "web_scheduling_user_interaction_tq";
    case MainThreadTaskQueue::QueueType::kWebSchedulingBestEffort:
      return "web_scheduling_background_tq";
    case MainThreadTaskQueue::QueueType::kCount:
      NOTREACHED();
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

MainThreadTaskQueue::QueueClass MainThreadTaskQueue::QueueClassForQueueType(
    QueueType type) {
  switch (type) {
    case QueueType::kControl:
    case QueueType::kDefault:
    case QueueType::kIdle:
    case QueueType::kTest:
    case QueueType::kV8:
    case QueueType::kIPC:
    case QueueType::kCleanup:
      return QueueClass::kNone;
    case QueueType::kFrameLoading:
    case QueueType::kFrameLoadingControl:
      return QueueClass::kLoading;
    case QueueType::kUnthrottled:
    case QueueType::kFrameThrottleable:
    case QueueType::kFrameDeferrable:
    case QueueType::kFramePausable:
    case QueueType::kFrameUnpausable:
    case QueueType::kWebSchedulingUserInteraction:
    case QueueType::kWebSchedulingBestEffort:
      return QueueClass::kTimer;
    case QueueType::kCompositor:
    case QueueType::kInput:
      return QueueClass::kCompositor;
    case QueueType::kDetached:
    case QueueType::kOther:
    case QueueType::kCount:
      DCHECK(false);
      return QueueClass::kCount;
  }
  NOTREACHED();
  return QueueClass::kNone;
}

MainThreadTaskQueue::MainThreadTaskQueue(
    std::unique_ptr<internal::TaskQueueImpl> impl,
    const TaskQueue::Spec& spec,
    const QueueCreationParams& params,
    MainThreadSchedulerImpl* main_thread_scheduler)
    : TaskQueue(std::move(impl), spec),
      queue_type_(params.queue_type),
      queue_class_(QueueClassForQueueType(params.queue_type)),
      fixed_priority_(params.fixed_priority),
      queue_traits_(params.queue_traits),
      freeze_when_keep_active_(params.freeze_when_keep_active),
      main_thread_scheduler_(main_thread_scheduler),
      frame_scheduler_(params.frame_scheduler) {
  if (GetTaskQueueImpl() && spec.should_notify_observers) {
    // TaskQueueImpl may be null for tests.
    // TODO(scheduler-dev): Consider mapping directly to
    // MainThreadSchedulerImpl::OnTaskStarted/Completed. At the moment this
    // is not possible due to task queue being created inside
    // MainThreadScheduler's constructor.
    GetTaskQueueImpl()->SetOnTaskStartedHandler(base::BindRepeating(
        &MainThreadTaskQueue::OnTaskStarted, base::Unretained(this)));
    GetTaskQueueImpl()->SetOnTaskCompletedHandler(base::BindRepeating(
        &MainThreadTaskQueue::OnTaskCompleted, base::Unretained(this)));
  }
}

MainThreadTaskQueue::~MainThreadTaskQueue() = default;

void MainThreadTaskQueue::OnTaskStarted(
    const base::sequence_manager::Task& task,
    const TaskQueue::TaskTiming& task_timing) {
  if (main_thread_scheduler_)
    main_thread_scheduler_->OnTaskStarted(this, task, task_timing);
}

void MainThreadTaskQueue::OnTaskCompleted(
    const base::sequence_manager::Task& task,
    const TaskQueue::TaskTiming& task_timing) {
  if (main_thread_scheduler_) {
    main_thread_scheduler_->OnTaskCompleted(this, task, task_timing);
  }
}

void MainThreadTaskQueue::DetachFromMainThreadScheduler() {
  // Frame has already been detached.
  if (!main_thread_scheduler_)
    return;

  if (GetTaskQueueImpl()) {
    GetTaskQueueImpl()->SetOnTaskStartedHandler(
        base::BindRepeating(&MainThreadSchedulerImpl::OnTaskStarted,
                            main_thread_scheduler_->GetWeakPtr(), nullptr));
    GetTaskQueueImpl()->SetOnTaskCompletedHandler(
        base::BindRepeating(&MainThreadSchedulerImpl::OnTaskCompleted,
                            main_thread_scheduler_->GetWeakPtr(), nullptr));
  }

  ClearReferencesToSchedulers();
}

void MainThreadTaskQueue::ShutdownTaskQueue() {
  ClearReferencesToSchedulers();
  TaskQueue::ShutdownTaskQueue();
}

void MainThreadTaskQueue::ClearReferencesToSchedulers() {
  if (main_thread_scheduler_)
    main_thread_scheduler_->OnShutdownTaskQueue(this);
  main_thread_scheduler_ = nullptr;
  frame_scheduler_ = nullptr;
}

FrameSchedulerImpl* MainThreadTaskQueue::GetFrameScheduler() const {
  return frame_scheduler_;
}

void MainThreadTaskQueue::DetachFromFrameScheduler() {
  frame_scheduler_ = nullptr;
}

void MainThreadTaskQueue::SetFrameSchedulerForTest(
    FrameSchedulerImpl* frame_scheduler) {
  frame_scheduler_ = frame_scheduler;
}

void MainThreadTaskQueue::SetNetRequestPriority(
    net::RequestPriority net_request_priority) {
  net_request_priority_ = net_request_priority;
}

base::Optional<net::RequestPriority> MainThreadTaskQueue::net_request_priority()
    const {
  return net_request_priority_;
}

}  // namespace scheduler
}  // namespace blink
