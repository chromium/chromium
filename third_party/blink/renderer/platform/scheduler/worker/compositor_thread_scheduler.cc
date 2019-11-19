// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/compositor_thread_scheduler.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/scheduler_helper.h"

namespace blink {
namespace scheduler {

namespace {

CompositorThreadScheduler* g_compositor_thread_scheduler = nullptr;

}  // namespace

// static
WebThreadScheduler* WebThreadScheduler::CompositorThreadScheduler() {
  return g_compositor_thread_scheduler;
}

CompositorThreadScheduler::CompositorThreadScheduler(
    base::sequence_manager::SequenceManager* sequence_manager)
    : NonMainThreadSchedulerImpl(sequence_manager,
                                 TaskType::kCompositorThreadTaskQueueDefault),
      input_task_queue_(
          base::FeatureList::IsEnabled(kHighPriorityInputOnCompositorThread)
              ? helper()->NewTaskQueue(
                    base::sequence_manager::TaskQueue::Spec("input_tq")
                        .SetShouldMonitorQuiescence(true))
              : nullptr),
      input_task_runner_(input_task_queue_
                             ? input_task_queue_->CreateTaskRunner(
                                   TaskType::kCompositorThreadTaskQueueInput)
                             : nullptr),
      compositor_metrics_helper_(helper()->HasCPUTimingForEachTask()) {
  if (input_task_queue_) {
    input_task_queue_->SetQueuePriority(
        base::sequence_manager::TaskQueue::QueuePriority::kHighestPriority);
  }
  DCHECK(!g_compositor_thread_scheduler);
  g_compositor_thread_scheduler = this;
}

CompositorThreadScheduler::~CompositorThreadScheduler() {
  DCHECK_EQ(g_compositor_thread_scheduler, this);
  g_compositor_thread_scheduler = nullptr;
}

scoped_refptr<NonMainThreadTaskQueue>
CompositorThreadScheduler::DefaultTaskQueue() {
  return helper()->DefaultNonMainThreadTaskQueue();
}

void CompositorThreadScheduler::InitImpl() {}

void CompositorThreadScheduler::OnTaskCompleted(
    NonMainThreadTaskQueue* worker_task_queue,
    const base::sequence_manager::Task& task,
    base::sequence_manager::TaskQueue::TaskTiming* task_timing,
    base::sequence_manager::LazyNow* lazy_now) {
  task_timing->RecordTaskEnd(lazy_now);
  compositor_metrics_helper_.RecordTaskMetrics(worker_task_queue, task,
                                               *task_timing);
}

scoped_refptr<scheduler::SingleThreadIdleTaskRunner>
CompositorThreadScheduler::IdleTaskRunner() {
  // TODO(flackr): This posts idle tasks as regular tasks. We need to create
  // an idle task runner with the semantics we want for the compositor thread
  // which runs them after the current frame has been drawn before the next
  // vsync. https://crbug.com/609532
  return base::MakeRefCounted<SingleThreadIdleTaskRunner>(
      helper()->DefaultTaskRunner(), this);
}

scoped_refptr<base::SingleThreadTaskRunner>
CompositorThreadScheduler::InputTaskRunner() {
  if (input_task_runner_)
    return input_task_runner_;
  return helper()->DefaultTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
CompositorThreadScheduler::V8TaskRunner() {
  NOTREACHED();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
CompositorThreadScheduler::CompositorTaskRunner() {
  NOTREACHED();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
CompositorThreadScheduler::IPCTaskRunner() {
  NOTREACHED();
  return nullptr;
}

bool CompositorThreadScheduler::CanExceedIdleDeadlineIfRequired() const {
  return false;
}

bool CompositorThreadScheduler::ShouldYieldForHighPriorityWork() {
  return false;
}

void CompositorThreadScheduler::AddTaskObserver(
    base::TaskObserver* task_observer) {
  helper()->AddTaskObserver(task_observer);
}

void CompositorThreadScheduler::RemoveTaskObserver(
    base::TaskObserver* task_observer) {
  helper()->RemoveTaskObserver(task_observer);
}

void CompositorThreadScheduler::Shutdown() {
  input_task_queue_->ShutdownTaskQueue();
}

void CompositorThreadScheduler::OnIdleTaskPosted() {}

base::TimeTicks CompositorThreadScheduler::WillProcessIdleTask() {
  // TODO(flackr): Return the next frame time as the deadline instead.
  // TODO(flackr): Ensure that oilpan GC does happen on the compositor thread
  // even though we will have no long idle periods. https://crbug.com/609531
  return base::TimeTicks::Now() + base::TimeDelta::FromMillisecondsD(16.7);
}

void CompositorThreadScheduler::DidProcessIdleTask() {}

base::TimeTicks CompositorThreadScheduler::NowTicks() {
  return base::TimeTicks::Now();
}

}  // namespace scheduler
}  // namespace blink
