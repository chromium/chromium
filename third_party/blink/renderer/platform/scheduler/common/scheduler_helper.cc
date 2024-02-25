// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/scheduler_helper.h"

#include <utility>

#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "third_party/blink/renderer/platform/scheduler/common/ukm_task_sampler.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::SequenceManager;
using base::sequence_manager::TaskQueue;
using base::sequence_manager::TaskTimeObserver;
using base::sequence_manager::TimeDomain;

SchedulerHelper::SchedulerHelper(SequenceManager* sequence_manager)
    : sequence_manager_(sequence_manager),
      observer_(nullptr),
      ukm_task_sampler_(sequence_manager_->GetMetricRecordingSettings()
                            .task_sampling_rate_for_recording_cpu_time) {
  sequence_manager_->SetWorkBatchSize(4);
}

void SchedulerHelper::InitDefaultTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  default_task_runner_ = std::move(task_runner);

  // Invoking SequenceManager::SetDefaultTaskRunner() before attaching the
  // SchedulerHelper to a thread is fine. The default TaskRunner will be stored
  // in TLS by the ThreadController before tasks are executed.
  DCHECK(sequence_manager_);
  sequence_manager_->SetDefaultTaskRunner(default_task_runner_);
}

void SchedulerHelper::AttachToCurrentThread() {
  DETACH_FROM_THREAD(thread_checker_);
  CheckOnValidThread();
  DCHECK(default_task_runner_)
      << "Must be invoked after InitDefaultTaskRunner().";
}

SchedulerHelper::~SchedulerHelper() {
  Shutdown();
}

void SchedulerHelper::Shutdown() {
  CheckOnValidThread();
  if (!sequence_manager_)
    return;
  ShutdownAllQueues();
  sequence_manager_->SetObserver(nullptr);
  sequence_manager_ = nullptr;
}

void SchedulerHelper::SetWorkBatchSizeForTesting(int work_batch_size) {
  CheckOnValidThread();
  DCHECK(sequence_manager_);
  sequence_manager_->SetWorkBatchSize(work_batch_size);
}

bool SchedulerHelper::GetAndClearSystemIsQuiescentBit() {
  CheckOnValidThread();
  DCHECK(sequence_manager_);
  return sequence_manager_->GetAndClearSystemIsQuiescentBit();
}

void SchedulerHelper::AddTaskObserver(base::TaskObserver* task_observer) {
  CheckOnValidThread();
  if (sequence_manager_) {
    static_cast<base::sequence_manager::internal::SequenceManagerImpl*>(
        sequence_manager_)
        ->AddTaskObserver(task_observer);
  }
}

void SchedulerHelper::RemoveTaskObserver(base::TaskObserver* task_observer) {
  CheckOnValidThread();
  if (sequence_manager_) {
    static_cast<base::sequence_manager::internal::SequenceManagerImpl*>(
        sequence_manager_)
        ->RemoveTaskObserver(task_observer);
  }
}

void SchedulerHelper::AddTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  if (sequence_manager_)
    sequence_manager_->AddTaskTimeObserver(task_time_observer);
}

void SchedulerHelper::RemoveTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  if (sequence_manager_)
    sequence_manager_->RemoveTaskTimeObserver(task_time_observer);
}

void SchedulerHelper::SetObserver(Observer* observer) {
  CheckOnValidThread();
  observer_ = observer;
  DCHECK(sequence_manager_);
  sequence_manager_->SetObserver(this);
}

void SchedulerHelper::ReclaimMemory() {
  CheckOnValidThread();
  DCHECK(sequence_manager_);
  sequence_manager_->ReclaimMemory();
}

std::optional<base::sequence_manager::WakeUp> SchedulerHelper::GetNextWakeUp()
    const {
  CheckOnValidThread();
  DCHECK(sequence_manager_);
  return sequence_manager_->GetNextDelayedWakeUp();
}

void SchedulerHelper::SetTimeDomain(
    base::sequence_manager::TimeDomain* time_domain) {
  CheckOnValidThread();
  DCHECK(sequence_manager_);
  return sequence_manager_->SetTimeDomain(time_domain);
}

void SchedulerHelper::ResetTimeDomain() {
  CheckOnValidThread();
  DCHECK(sequence_manager_);
  return sequence_manager_->ResetTimeDomain();
}

void SchedulerHelper::OnBeginNestedRunLoop() {
  ++nested_runloop_depth_;
  if (observer_)
    observer_->OnBeginNestedRunLoop();
}

void SchedulerHelper::OnExitNestedRunLoop() {
  --nested_runloop_depth_;
  DCHECK_GE(nested_runloop_depth_, 0);
  if (observer_)
    observer_->OnExitNestedRunLoop();
}

const base::TickClock* SchedulerHelper::GetClock() const {
  if (sequence_manager_)
    return sequence_manager_->GetTickClock();
  return nullptr;
}

base::TimeTicks SchedulerHelper::NowTicks() const {
  if (sequence_manager_)
    return sequence_manager_->NowTicks();
  // We may need current time for tracing when shutting down worker thread.
  return base::TimeTicks::Now();
}

bool SchedulerHelper::HasCPUTimingForEachTask() const {
  if (sequence_manager_) {
    return sequence_manager_->GetMetricRecordingSettings()
        .records_cpu_time_for_all_tasks();
  }
  return false;
}

}  // namespace scheduler
}  // namespace blink
