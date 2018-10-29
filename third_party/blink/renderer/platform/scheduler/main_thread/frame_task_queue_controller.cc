// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_task_queue_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/trace_event/traced_value.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;
using QueueTraits = MainThreadTaskQueue::QueueTraits;
using QueueEnabledVoter = base::sequence_manager::TaskQueue::QueueEnabledVoter;

FrameTaskQueueController::FrameTaskQueueController(
    MainThreadSchedulerImpl* main_thread_scheduler_impl,
    FrameSchedulerImpl* frame_scheduler_impl,
    Delegate* delegate)
    : main_thread_scheduler_impl_(main_thread_scheduler_impl),
      frame_scheduler_impl_(frame_scheduler_impl),
      delegate_(delegate) {
  DCHECK(frame_scheduler_impl_);
  DCHECK(delegate_);
}

FrameTaskQueueController::~FrameTaskQueueController() = default;

scoped_refptr<MainThreadTaskQueue>
FrameTaskQueueController::LoadingTaskQueue() {
  if (!loading_task_queue_)
    CreateLoadingTaskQueue();
  DCHECK(loading_task_queue_);
  return loading_task_queue_;
}

scoped_refptr<MainThreadTaskQueue>
FrameTaskQueueController::LoadingControlTaskQueue() {
  if (!loading_control_task_queue_)
    CreateLoadingControlTaskQueue();
  DCHECK(loading_control_task_queue_);
  return loading_control_task_queue_;
}

scoped_refptr<MainThreadTaskQueue>
FrameTaskQueueController::InspectorTaskQueue() {
  if (!inspector_task_queue_) {
    inspector_task_queue_ = main_thread_scheduler_impl_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kDefault)
            .SetFrameScheduler(frame_scheduler_impl_));
    TaskQueueCreated(inspector_task_queue_);
  }
  return inspector_task_queue_;
}

scoped_refptr<MainThreadTaskQueue>
FrameTaskQueueController::ExperimentalWebSchedulingTaskQueue(
    WebSchedulingTaskQueueType task_queue_type) {
  if (!web_scheduling_task_queues_[task_queue_type])
    CreateWebSchedulingTaskQueue(task_queue_type);

  DCHECK(web_scheduling_task_queues_[task_queue_type]);
  return web_scheduling_task_queues_[task_queue_type];
}

scoped_refptr<MainThreadTaskQueue>
FrameTaskQueueController::NonLoadingTaskQueue(
    MainThreadTaskQueue::QueueTraits queue_traits) {
  if (!non_loading_task_queues_.Contains(queue_traits.Key()))
    CreateNonLoadingTaskQueue(queue_traits);
  auto it = non_loading_task_queues_.find(queue_traits.Key());
  DCHECK(it != non_loading_task_queues_.end());
  return it->value;
}

const std::vector<FrameTaskQueueController::TaskQueueAndEnabledVoterPair>&
FrameTaskQueueController::GetAllTaskQueuesAndVoters() const {
  return all_task_queues_and_voters_;
}

void FrameTaskQueueController::CreateLoadingTaskQueue() {
  DCHECK(!loading_task_queue_);
  // |main_thread_scheduler_impl_| can be null in unit tests.
  DCHECK(main_thread_scheduler_impl_);

  loading_task_queue_ = main_thread_scheduler_impl_->NewLoadingTaskQueue(
      MainThreadTaskQueue::QueueType::kFrameLoading, frame_scheduler_impl_);
  TaskQueueCreated(loading_task_queue_);
}

void FrameTaskQueueController::CreateWebSchedulingTaskQueue(
    WebSchedulingTaskQueueType task_queue_type) {
  DCHECK(RuntimeEnabledFeatures::WorkerTaskQueueEnabled());
  DCHECK(!web_scheduling_task_queues_[task_queue_type]);
  // |main_thread_scheduler_impl_| can be null in unit tests.
  DCHECK(main_thread_scheduler_impl_);

  MainThreadTaskQueue::QueueType main_thread_queue_type =
      MainThreadTaskQueue::QueueType::kDefault;
  switch (task_queue_type) {
    case kWebSchedulingUserVisiblePriority:
      main_thread_queue_type =
          MainThreadTaskQueue::QueueType::kWebSchedulingUserInteraction;
      break;
    case kWebSchedulingBestEffortPriority:
      main_thread_queue_type =
          MainThreadTaskQueue::QueueType::kWebSchedulingBestEffort;
      break;
    case kWebSchedulingPriorityCount:
      NOTREACHED();
  }

  scoped_refptr<MainThreadTaskQueue> task_queue =
      main_thread_scheduler_impl_->NewTaskQueue(
          MainThreadTaskQueue::QueueCreationParams(main_thread_queue_type)
              .SetCanBePaused(true)
              .SetCanBeFrozen(true)
              .SetCanBeDeferred(task_queue_type !=
                                kWebSchedulingUserVisiblePriority)
              .SetCanBeThrottled(true)
              .SetFrameScheduler(frame_scheduler_impl_));

  TaskQueueCreated(task_queue);
  web_scheduling_task_queues_[task_queue_type] = task_queue;
}

void FrameTaskQueueController::CreateLoadingControlTaskQueue() {
  DCHECK(!loading_control_task_queue_);
  // |main_thread_scheduler_impl_| can be null in unit tests.
  DCHECK(main_thread_scheduler_impl_);

  loading_control_task_queue_ =
      main_thread_scheduler_impl_->NewLoadingTaskQueue(
          MainThreadTaskQueue::QueueType::kFrameLoadingControl,
          frame_scheduler_impl_);
  TaskQueueCreated(loading_control_task_queue_);
}

scoped_refptr<MainThreadTaskQueue>
FrameTaskQueueController::NewResourceLoadingTaskQueue() {
  scoped_refptr<MainThreadTaskQueue> task_queue =
      main_thread_scheduler_impl_->NewLoadingTaskQueue(
          MainThreadTaskQueue::QueueType::kFrameLoading, frame_scheduler_impl_);
  TaskQueueCreated(task_queue);
  resource_loading_task_queues_.insert(task_queue);
  return task_queue;
}

void FrameTaskQueueController::CreateNonLoadingTaskQueue(
    QueueTraits queue_traits) {
  DCHECK(!non_loading_task_queues_.Contains(queue_traits.Key()));
  // |main_thread_scheduler_impl_| can be null in unit tests.
  DCHECK(main_thread_scheduler_impl_);

  scoped_refptr<MainThreadTaskQueue> task_queue =
      main_thread_scheduler_impl_->NewTaskQueue(
          MainThreadTaskQueue::QueueCreationParams(
              QueueTypeFromQueueTraits(queue_traits))
              .SetQueueTraits(queue_traits)
              // Freeze when keep active is currently only set for the
              // throttleable queue.
              // TODO(altimin): Figure out how to set this for new queues.
              // Investigate which tasks must be kept alive, and if possible
              // move them to an unfreezable queue and remove this override and
              // the page scheduler KeepActive freezing override.
              .SetFreezeWhenKeepActive(queue_traits.can_be_throttled)
              .SetFrameScheduler(frame_scheduler_impl_));
  TaskQueueCreated(task_queue);
  non_loading_task_queues_.insert(queue_traits.Key(), task_queue);
}

void FrameTaskQueueController::TaskQueueCreated(
    const scoped_refptr<MainThreadTaskQueue>& task_queue) {
  DCHECK(task_queue);

  std::unique_ptr<QueueEnabledVoter> voter;
  // Only create a voter for queues that can be disabled.
  if (task_queue->CanBePaused() || task_queue->CanBeFrozen())
    voter = task_queue->CreateQueueEnabledVoter();

  delegate_->OnTaskQueueCreated(task_queue.get(), voter.get());

  all_task_queues_and_voters_.push_back(
      TaskQueueAndEnabledVoterPair(task_queue.get(), voter.get()));

  if (voter) {
    DCHECK(task_queue_enabled_voters_.find(task_queue) ==
           task_queue_enabled_voters_.end());
    task_queue_enabled_voters_.insert(task_queue, std::move(voter));
  }
}

base::sequence_manager::TaskQueue::QueueEnabledVoter*
FrameTaskQueueController::GetQueueEnabledVoter(
    const scoped_refptr<MainThreadTaskQueue>& task_queue) {
  auto it = task_queue_enabled_voters_.find(task_queue);
  if (it == task_queue_enabled_voters_.end())
    return nullptr;
  return it->value.get();
}

bool FrameTaskQueueController::RemoveResourceLoadingTaskQueue(
    const scoped_refptr<MainThreadTaskQueue>& task_queue) {
  DCHECK(task_queue);

  if (!resource_loading_task_queues_.Contains(task_queue))
    return false;
  resource_loading_task_queues_.erase(task_queue);
  DCHECK(task_queue_enabled_voters_.Contains(task_queue));
  task_queue_enabled_voters_.erase(task_queue);

  bool found_task_queue = false;
  for (auto it = all_task_queues_and_voters_.begin();
       it != all_task_queues_and_voters_.end(); ++it) {
    if (it->first == task_queue.get()) {
      found_task_queue = true;
      all_task_queues_and_voters_.erase(it);
      break;
    }
  }
  DCHECK(found_task_queue);
  return true;
}

void FrameTaskQueueController::AsValueInto(
    base::trace_event::TracedValue* state) const {
  if (loading_task_queue_) {
    state->SetString("loading_task_queue",
                     PointerToString(loading_task_queue_.get()));
  }
  if (loading_control_task_queue_) {
    state->SetString("loading_control_task_queue",
                     PointerToString(loading_control_task_queue_.get()));
  }
  state->BeginArray("non_loading_task_queues");
  for (const auto it : non_loading_task_queues_) {
    state->AppendString(PointerToString(it.value.get()));
  }
  state->EndArray();

  state->BeginArray("resource_loading_task_queues");
  for (const auto& queue : resource_loading_task_queues_) {
    state->AppendString(PointerToString(queue.get()));
  }
  state->EndArray();
}

// static
MainThreadTaskQueue::QueueType
FrameTaskQueueController::QueueTypeFromQueueTraits(QueueTraits queue_traits) {
  if (queue_traits.can_be_throttled)
    return MainThreadTaskQueue::QueueType::kFrameThrottleable;
  if (queue_traits.can_be_deferred)
    return MainThreadTaskQueue::QueueType::kFrameDeferrable;
  if (queue_traits.can_be_paused)
    return MainThreadTaskQueue::QueueType::kFramePausable;
  return MainThreadTaskQueue::QueueType::kFrameUnpausable;
}

}  // namespace scheduler
}  // namespace blink
