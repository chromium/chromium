// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/scheduler_helper.h"

#include <utility>

#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_queue.h"
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

void SchedulerHelper::InitDefaultQueues(
    scoped_refptr<TaskQueue> default_task_queue,
    scoped_refptr<TaskQueue> control_task_queue,
    TaskType default_task_type) {
  control_task_queue->SetQueuePriority(TaskQueue::kControlPriority);

  default_task_runner_ =
      default_task_queue->CreateTaskRunner(static_cast<int>(default_task_type));

  DCHECK(sequence_manager_);
  sequence_manager_->SetDefaultTaskRunner(default_task_runner_);

  blink_task_executor_.emplace(default_task_runner_, sequence_manager_);
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

scoped_refptr<base::SingleThreadTaskRunner>
SchedulerHelper::DefaultTaskRunner() {
  return default_task_runner_;
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

TimeDomain* SchedulerHelper::real_time_domain() const {
  CheckOnValidThread();
  DCHECK(sequence_manager_);
  return sequence_manager_->GetRealTimeDomain();
}

void SchedulerHelper::RegisterTimeDomain(TimeDomain* time_domain) {
  CheckOnValidThread();
  DCHECK(sequence_manager_);
  sequence_manager_->RegisterTimeDomain(time_domain);
}

void SchedulerHelper::UnregisterTimeDomain(TimeDomain* time_domain) {
  CheckOnValidThread();
  if (sequence_manager_)
    sequence_manager_->UnregisterTimeDomain(time_domain);
}

void SchedulerHelper::OnBeginNestedRunLoop() {
  if (observer_)
    observer_->OnBeginNestedRunLoop();
}

void SchedulerHelper::OnExitNestedRunLoop() {
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

void SchedulerHelper::SetTimerSlack(base::TimerSlack timer_slack) {
  if (sequence_manager_) {
    static_cast<base::sequence_manager::internal::SequenceManagerImpl*>(
        sequence_manager_)
        ->SetTimerSlack(timer_slack);
  }
}

double SchedulerHelper::GetSamplingRateForRecordingCPUTime() const {
  if (sequence_manager_) {
    return sequence_manager_->GetMetricRecordingSettings()
        .task_sampling_rate_for_recording_cpu_time;
  }
  return 0;
}

bool SchedulerHelper::HasCPUTimingForEachTask() const {
  if (sequence_manager_) {
    return sequence_manager_->GetMetricRecordingSettings()
        .records_cpu_time_for_all_tasks();
  }
  return false;
}

SchedulerHelper::BlinkTaskExecutor::BlinkTaskExecutor(
    scoped_refptr<base::SingleThreadTaskRunner> default_task_queue,
    base::sequence_manager::SequenceManager* sequence_manager)
    : base::SimpleTaskExecutor(sequence_manager, std::move(default_task_queue)),
      sequence_manager_(sequence_manager) {}

SchedulerHelper::BlinkTaskExecutor::~BlinkTaskExecutor() = default;

const scoped_refptr<base::SequencedTaskRunner>&
SchedulerHelper::BlinkTaskExecutor::GetContinuationTaskRunner() {
  return sequence_manager_->GetTaskRunnerForCurrentTask();
}

}  // namespace scheduler
}  // namespace blink
