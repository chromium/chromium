// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/compositor_thread_scheduler_impl.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/scheduler_helper.h"

namespace blink {

namespace {

scheduler::CompositorThreadSchedulerImpl* g_compositor_thread_scheduler =
    nullptr;

}  // namespace

// static
blink::CompositorThreadScheduler* ThreadScheduler::CompositorThreadScheduler() {
  return g_compositor_thread_scheduler;
}

namespace scheduler {

CompositorThreadSchedulerImpl::CompositorThreadSchedulerImpl(
    base::sequence_manager::SequenceManager* sequence_manager)
    : NonMainThreadSchedulerBase(sequence_manager,
                                 TaskType::kCompositorThreadTaskQueueDefault),
      compositor_metrics_helper_(GetHelper().HasCPUTimingForEachTask()) {
  DCHECK(!g_compositor_thread_scheduler);
  g_compositor_thread_scheduler = this;
}

CompositorThreadSchedulerImpl::~CompositorThreadSchedulerImpl() {
  DCHECK_EQ(g_compositor_thread_scheduler, this);
  g_compositor_thread_scheduler = nullptr;
}

scoped_refptr<NonMainThreadTaskQueue>
CompositorThreadSchedulerImpl::DefaultTaskQueue() {
  return GetHelper().DefaultNonMainThreadTaskQueue();
}

void CompositorThreadSchedulerImpl::OnTaskCompleted(
    NonMainThreadTaskQueue* worker_task_queue,
    const base::sequence_manager::Task& task,
    base::sequence_manager::TaskQueue::TaskTiming* task_timing,
    base::LazyNow* lazy_now) {
  task_timing->RecordTaskEnd(lazy_now);
  DispatchOnTaskCompletionCallbacks();
  compositor_metrics_helper_.RecordTaskMetrics(task, *task_timing);
}

scoped_refptr<scheduler::SingleThreadIdleTaskRunner>
CompositorThreadSchedulerImpl::IdleTaskRunner() {
  // TODO(flackr): This posts idle tasks as regular tasks. We need to create
  // an idle task runner with the semantics we want for the compositor thread
  // which runs them after the current frame has been drawn before the next
  // vsync. https://crbug.com/609532
  return base::MakeRefCounted<SingleThreadIdleTaskRunner>(
      GetHelper().DefaultTaskRunner(), GetHelper().ControlTaskRunner(), this);
}

scoped_refptr<base::SingleThreadTaskRunner>
CompositorThreadSchedulerImpl::V8TaskRunner() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
CompositorThreadSchedulerImpl::CleanupTaskRunner() {
  return DefaultTaskQueue()->GetTaskRunnerWithDefaultTaskType();
}

scoped_refptr<base::SingleThreadTaskRunner>
CompositorThreadSchedulerImpl::InputTaskRunner() {
  return GetHelper().InputTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
CompositorThreadSchedulerImpl::DefaultTaskRunner() {
  return GetHelper().DefaultTaskRunner();
}

bool CompositorThreadSchedulerImpl::ShouldYieldForHighPriorityWork() {
  return false;
}

void CompositorThreadSchedulerImpl::AddTaskObserver(
    base::TaskObserver* task_observer) {
  GetHelper().AddTaskObserver(task_observer);
}

void CompositorThreadSchedulerImpl::RemoveTaskObserver(
    base::TaskObserver* task_observer) {
  GetHelper().RemoveTaskObserver(task_observer);
}

void CompositorThreadSchedulerImpl::Shutdown() {}

void CompositorThreadSchedulerImpl::OnIdleTaskPosted() {}

base::TimeTicks CompositorThreadSchedulerImpl::WillProcessIdleTask() {
  // TODO(flackr): Return the next frame time as the deadline instead.
  // TODO(flackr): Ensure that oilpan GC does happen on the compositor thread
  // even though we will have no long idle periods. https://crbug.com/609531
  return base::TimeTicks::Now() + base::Milliseconds(16.7);
}

void CompositorThreadSchedulerImpl::DidProcessIdleTask() {}

base::TimeTicks CompositorThreadSchedulerImpl::NowTicks() {
  return base::TimeTicks::Now();
}

void CompositorThreadSchedulerImpl::PostIdleTask(const base::Location& location,
                                                 Thread::IdleTask task) {
  IdleTaskRunner()->PostIdleTask(location, std::move(task));
}

void CompositorThreadSchedulerImpl::PostNonNestableIdleTask(
    const base::Location& location,
    Thread::IdleTask task) {
  IdleTaskRunner()->PostNonNestableIdleTask(location, std::move(task));
}

void CompositorThreadSchedulerImpl::PostDelayedIdleTask(
    const base::Location& location,
    base::TimeDelta delay,
    Thread::IdleTask task) {
  IdleTaskRunner()->PostDelayedIdleTask(location, delay, std::move(task));
}

base::TimeTicks
CompositorThreadSchedulerImpl::MonotonicallyIncreasingVirtualTime() {
  return NowTicks();
}

void CompositorThreadSchedulerImpl::SetV8Isolate(v8::Isolate* isolate) {
  NonMainThreadSchedulerBase::SetV8Isolate(isolate);
}

}  // namespace scheduler
}  // namespace blink
