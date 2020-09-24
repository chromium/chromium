// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/task/common/scoped_defer_task_posting.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

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
    case MainThreadTaskQueue::QueueType::kInput:
      return "input_tq";
    case MainThreadTaskQueue::QueueType::kDetached:
      return "detached_tq";
    case MainThreadTaskQueue::QueueType::kOther:
      return "other_tq";
    case MainThreadTaskQueue::QueueType::kWebScheduling:
      return "web_scheduling_tq";
    case MainThreadTaskQueue::QueueType::kNonWaking:
      return "non_waking_tq";
    case MainThreadTaskQueue::QueueType::kCount:
      NOTREACHED();
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

// static
bool MainThreadTaskQueue::IsPerFrameTaskQueue(
    MainThreadTaskQueue::QueueType queue_type) {
  switch (queue_type) {
    // TODO(altimin): Remove kDefault once there is no per-frame kDefault queue.
    case MainThreadTaskQueue::QueueType::kDefault:
    case MainThreadTaskQueue::QueueType::kFrameLoading:
    case MainThreadTaskQueue::QueueType::kFrameLoadingControl:
    case MainThreadTaskQueue::QueueType::kFrameThrottleable:
    case MainThreadTaskQueue::QueueType::kFrameDeferrable:
    case MainThreadTaskQueue::QueueType::kFramePausable:
    case MainThreadTaskQueue::QueueType::kFrameUnpausable:
    case MainThreadTaskQueue::QueueType::kIdle:
    case MainThreadTaskQueue::QueueType::kWebScheduling:
      return true;
    case MainThreadTaskQueue::QueueType::kControl:
    case MainThreadTaskQueue::QueueType::kCompositor:
    case MainThreadTaskQueue::QueueType::kTest:
    case MainThreadTaskQueue::QueueType::kV8:
    case MainThreadTaskQueue::QueueType::kInput:
    case MainThreadTaskQueue::QueueType::kDetached:
    case MainThreadTaskQueue::QueueType::kNonWaking:
    case MainThreadTaskQueue::QueueType::kOther:
      return false;
    case MainThreadTaskQueue::QueueType::kCount:
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
}

MainThreadTaskQueue::MainThreadTaskQueue(
    std::unique_ptr<internal::TaskQueueImpl> impl,
    const TaskQueue::Spec& spec,
    const QueueCreationParams& params,
    MainThreadSchedulerImpl* main_thread_scheduler)
    : TaskQueue(std::move(impl), spec),
      queue_type_(params.queue_type),
      queue_traits_(params.queue_traits),
      freeze_when_keep_active_(params.freeze_when_keep_active),
      web_scheduling_priority_(params.web_scheduling_priority),
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
    TaskQueue::TaskTiming* task_timing,
    base::sequence_manager::LazyNow* lazy_now) {
  if (main_thread_scheduler_) {
    main_thread_scheduler_->OnTaskCompleted(weak_ptr_factory_.GetWeakPtr(),
                                            task, task_timing, lazy_now);
  }
}

void MainThreadTaskQueue::DetachFromMainThreadScheduler() {
  weak_ptr_factory_.InvalidateWeakPtrs();

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
    GetTaskQueueImpl()->SetOnTaskPostedHandler(
        internal::TaskQueueImpl::OnTaskPostedHandler());
  }

  ClearReferencesToSchedulers();
}

void MainThreadTaskQueue::SetOnIPCTaskPosted(
    base::RepeatingCallback<void(const base::sequence_manager::Task&)>
        on_ipc_task_posted_callback) {
  if (GetTaskQueueImpl()) {
    // We use the frame_scheduler_ to track metrics so as to ensure that metrics
    // are not tied to individual task queues.
    GetTaskQueueImpl()->SetOnTaskPostedHandler(on_ipc_task_posted_callback);
  }
}

void MainThreadTaskQueue::DetachOnIPCTaskPostedWhileInBackForwardCache() {
  if (GetTaskQueueImpl()) {
    GetTaskQueueImpl()->SetOnTaskPostedHandler(
        internal::TaskQueueImpl::OnTaskPostedHandler());
  }
}

void MainThreadTaskQueue::ShutdownTaskQueue() {
  ClearReferencesToSchedulers();
  TaskQueue::ShutdownTaskQueue();
}

AgentGroupSchedulerImpl* MainThreadTaskQueue::GetAgentGroupScheduler() {
  DCHECK(task_runner()->BelongsToCurrentThread());
  if (agent_group_scheduler_) {
    DCHECK(!frame_scheduler_);
    return agent_group_scheduler_;
  }
  if (frame_scheduler_) {
    return frame_scheduler_->GetAgentGroupScheduler();
  }
  // If this MainThreadTaskQueue was created for MainThreadSchedulerImpl, this
  // queue will not be associated with AgentGroupScheduler or FrameScheduler.
  return nullptr;
}

void MainThreadTaskQueue::ClearReferencesToSchedulers() {
  if (main_thread_scheduler_)
    main_thread_scheduler_->OnShutdownTaskQueue(this);
  main_thread_scheduler_ = nullptr;
  agent_group_scheduler_ = nullptr;
  frame_scheduler_ = nullptr;
}

FrameSchedulerImpl* MainThreadTaskQueue::GetFrameScheduler() const {
  DCHECK(task_runner()->BelongsToCurrentThread());
  return frame_scheduler_;
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

void MainThreadTaskQueue::SetWebSchedulingPriority(
    WebSchedulingPriority priority) {
  if (web_scheduling_priority_ == priority)
    return;
  web_scheduling_priority_ = priority;
  frame_scheduler_->OnWebSchedulingTaskQueuePriorityChanged(this);
}

base::Optional<WebSchedulingPriority>
MainThreadTaskQueue::web_scheduling_priority() const {
  return web_scheduling_priority_;
}

}  // namespace scheduler
}  // namespace blink
