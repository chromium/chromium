// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/scheduler.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/hash/md5_constexpr.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_preferences.h"

namespace gpu {

namespace {

uint64_t GetTaskFlowId(uint32_t sequence_id, uint32_t order_num) {
  // Xor with a mask to ensure that the flow id does not collide with non-gpu
  // tasks.
  static constexpr uint64_t kMask = base::MD5Hash64Constexpr("gpu::Scheduler");
  return kMask ^ (sequence_id) ^ (static_cast<uint64_t>(order_num) << 32);
}

}  // namespace

Scheduler::Task::Task(SequenceId sequence_id,
                      base::OnceClosure closure,
                      std::vector<SyncToken> sync_token_fences)
    : sequence_id(sequence_id),
      closure(std::move(closure)),
      sync_token_fences(std::move(sync_token_fences)) {}
Scheduler::Task::Task(Task&& other) = default;
Scheduler::Task::~Task() = default;
Scheduler::Task& Scheduler::Task::operator=(Task&& other) = default;

Scheduler::SchedulingState::SchedulingState() = default;
Scheduler::SchedulingState::SchedulingState(const SchedulingState& other) =
    default;
Scheduler::SchedulingState::~SchedulingState() = default;

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
Scheduler::SchedulingState::AsValue() const {
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  state->SetInteger("sequence_id", sequence_id.GetUnsafeValue());
  state->SetString("priority", SchedulingPriorityToString(priority));
  state->SetInteger("order_num", order_num);
  return std::move(state);
}

Scheduler::Sequence::Task::Task(base::OnceClosure closure, uint32_t order_num)
    : closure(std::move(closure)), order_num(order_num) {}
Scheduler::Sequence::Task::Task(Task&& other) = default;
Scheduler::Sequence::Task::~Task() = default;
Scheduler::Sequence::Task& Scheduler::Sequence::Task::operator=(Task&& other) =
    default;

Scheduler::Sequence::WaitFence::WaitFence(const SyncToken& sync_token,
                                          uint32_t order_num,
                                          SequenceId release_sequence_id)
    : sync_token(sync_token),
      order_num(order_num),
      release_sequence_id(release_sequence_id) {}
Scheduler::Sequence::WaitFence::WaitFence(WaitFence&& other) = default;
Scheduler::Sequence::WaitFence::~WaitFence() = default;
Scheduler::Sequence::WaitFence& Scheduler::Sequence::WaitFence::operator=(
    WaitFence&& other) = default;

Scheduler::Sequence::Sequence(Scheduler* scheduler,
                              SequenceId sequence_id,
                              SchedulingPriority priority,
                              scoped_refptr<SyncPointOrderData> order_data)
    : scheduler_(scheduler),
      sequence_id_(sequence_id),
      default_priority_(priority),
      current_priority_(priority),
      order_data_(std::move(order_data)) {}

Scheduler::Sequence::~Sequence() {
  for (auto& kv : wait_fences_) {
    Sequence* release_sequence =
        scheduler_->GetSequence(kv.first.release_sequence_id);
    if (release_sequence)
      release_sequence->RemoveWaitingPriority(kv.second);
  }

  order_data_->Destroy();
}

void Scheduler::Sequence::UpdateSchedulingPriority() {
  SchedulingPriority priority = default_priority_;
  if (!client_waits_.empty())
    priority = std::min(priority, SchedulingPriority::kHigh);

  for (int release_priority = 0; release_priority < static_cast<int>(priority);
       release_priority++) {
    if (waiting_priority_counts_[release_priority] != 0) {
      priority = static_cast<SchedulingPriority>(release_priority);
      break;
    }
  }

  if (current_priority_ != priority) {
    TRACE_EVENT2("gpu", "Scheduler::Sequence::UpdateSchedulingPriority",
                 "sequence_id", sequence_id_.GetUnsafeValue(), "new_priority",
                 SchedulingPriorityToString(priority));

    current_priority_ = priority;
    scheduler_->TryScheduleSequence(this);
  }
}

bool Scheduler::Sequence::NeedsRescheduling() const {
  return (running_state_ != IDLE &&
          scheduling_state_.priority != current_priority()) ||
         (running_state_ == SCHEDULED && !IsRunnable());
}

bool Scheduler::Sequence::IsRunnable() const {
  return enabled_ && !tasks_.empty() &&
         (wait_fences_.empty() ||
          wait_fences_.begin()->first.order_num > tasks_.front().order_num);
}

bool Scheduler::Sequence::ShouldYieldTo(const Sequence* other) const {
  if (!running() || !other->scheduled())
    return false;
  return other->scheduling_state_.RunsBefore(scheduling_state_);
}

void Scheduler::Sequence::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;
  if (enabled) {
    TRACE_EVENT_ASYNC_BEGIN1("gpu", "SequenceEnabled", this, "sequence_id",
                             sequence_id_.GetUnsafeValue());
  } else {
    TRACE_EVENT_ASYNC_END1("gpu", "SequenceEnabled", this, "sequence_id",
                           sequence_id_.GetUnsafeValue());
  }
  scheduler_->TryScheduleSequence(this);
}

Scheduler::SchedulingState Scheduler::Sequence::SetScheduled() {
  DCHECK(IsRunnable());
  DCHECK_NE(running_state_, RUNNING);

  running_state_ = SCHEDULED;

  scheduling_state_.sequence_id = sequence_id_;
  scheduling_state_.priority = current_priority();
  scheduling_state_.order_num = tasks_.front().order_num;

  return scheduling_state_;
}

void Scheduler::Sequence::UpdateRunningPriority() {
  DCHECK_EQ(running_state_, RUNNING);
  scheduling_state_.priority = current_priority();
}

void Scheduler::Sequence::ContinueTask(base::OnceClosure closure) {
  DCHECK_EQ(running_state_, RUNNING);
  uint32_t order_num = order_data_->current_order_num();
  tasks_.push_front({std::move(closure), order_num});
  order_data_->PauseProcessingOrderNumber(order_num);
}

uint32_t Scheduler::Sequence::ScheduleTask(base::OnceClosure closure) {
  uint32_t order_num = order_data_->GenerateUnprocessedOrderNumber();
  TRACE_EVENT_WITH_FLOW0("gpu,toplevel.flow", "Scheduler::ScheduleTask",
                         GetTaskFlowId(sequence_id_.value(), order_num),
                         TRACE_EVENT_FLAG_FLOW_OUT);
  tasks_.push_back({std::move(closure), order_num});
  return order_num;
}

uint32_t Scheduler::Sequence::BeginTask(base::OnceClosure* closure) {
  DCHECK(closure);
  DCHECK(!tasks_.empty());
  DCHECK_EQ(running_state_, SCHEDULED);

  running_state_ = RUNNING;

  *closure = std::move(tasks_.front().closure);
  uint32_t order_num = tasks_.front().order_num;
  tasks_.pop_front();

  return order_num;
}

void Scheduler::Sequence::FinishTask() {
  DCHECK_EQ(running_state_, RUNNING);
  running_state_ = IDLE;
}

void Scheduler::Sequence::AddWaitFence(const SyncToken& sync_token,
                                       uint32_t order_num,
                                       SequenceId release_sequence_id) {
  auto it =
      wait_fences_.find(WaitFence{sync_token, order_num, release_sequence_id});
  if (it != wait_fences_.end())
    return;

  // |release_sequence| can be nullptr if we wait on SyncToken from sequence
  // that is not in this scheduler. It can happen on WebView when compositing
  // that runs on different thread returns resources.
  Sequence* release_sequence = scheduler_->GetSequence(release_sequence_id);
  if (release_sequence)
    release_sequence->AddWaitingPriority(default_priority_);

  wait_fences_.emplace(
      std::make_pair(WaitFence(sync_token, order_num, release_sequence_id),
                     default_priority_));
}

void Scheduler::Sequence::RemoveWaitFence(const SyncToken& sync_token,
                                          uint32_t order_num,
                                          SequenceId release_sequence_id) {
  auto it =
      wait_fences_.find(WaitFence{sync_token, order_num, release_sequence_id});
  if (it != wait_fences_.end()) {
    SchedulingPriority wait_priority = it->second;
    wait_fences_.erase(it);

    Sequence* release_sequence = scheduler_->GetSequence(release_sequence_id);
    if (release_sequence)
      release_sequence->RemoveWaitingPriority(wait_priority);

    scheduler_->TryScheduleSequence(this);
  }
}

void Scheduler::Sequence::PropagatePriority(SchedulingPriority priority) {
  for (auto& kv : wait_fences_) {
    if (kv.second > priority) {
      SchedulingPriority old_priority = kv.second;
      kv.second = priority;

      Sequence* release_sequence =
          scheduler_->GetSequence(kv.first.release_sequence_id);
      if (release_sequence) {
        release_sequence->ChangeWaitingPriority(old_priority, priority);
      }
    }
  }
}

void Scheduler::Sequence::AddWaitingPriority(SchedulingPriority priority) {
  TRACE_EVENT2("gpu", "Scheduler::Sequence::RemoveWaitingPriority",
               "sequence_id", sequence_id_.GetUnsafeValue(), "new_priority",
               SchedulingPriorityToString(priority));

  waiting_priority_counts_[static_cast<int>(priority)]++;

  if (priority < current_priority_) {
    UpdateSchedulingPriority();
  }

  PropagatePriority(priority);
}

void Scheduler::Sequence::RemoveWaitingPriority(SchedulingPriority priority) {
  TRACE_EVENT2("gpu", "Scheduler::Sequence::RemoveWaitingPriority",
               "sequence_id", sequence_id_.GetUnsafeValue(), "new_priority",
               SchedulingPriorityToString(priority));

  DCHECK(waiting_priority_counts_[static_cast<int>(priority)] > 0);
  waiting_priority_counts_[static_cast<int>(priority)]--;

  if (priority == current_priority_ &&
      waiting_priority_counts_[static_cast<int>(priority)] == 0)
    UpdateSchedulingPriority();
}

void Scheduler::Sequence::ChangeWaitingPriority(
    SchedulingPriority old_priority,
    SchedulingPriority new_priority) {
  DCHECK(waiting_priority_counts_[static_cast<int>(old_priority)] != 0);
  waiting_priority_counts_[static_cast<int>(old_priority)]--;
  waiting_priority_counts_[static_cast<int>(new_priority)]++;

  if (new_priority < current_priority_ ||
      (old_priority == current_priority_ &&
       waiting_priority_counts_[static_cast<int>(old_priority)] == 0)) {
    UpdateSchedulingPriority();
  }

  PropagatePriority(new_priority);
}

void Scheduler::Sequence::AddClientWait(CommandBufferId command_buffer_id) {
  client_waits_.insert(command_buffer_id);
  UpdateSchedulingPriority();
  PropagatePriority(SchedulingPriority::kHigh);
}

void Scheduler::Sequence::RemoveClientWait(CommandBufferId command_buffer_id) {
  client_waits_.erase(command_buffer_id);
  UpdateSchedulingPriority();
}

Scheduler::Scheduler(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                     SyncPointManager* sync_point_manager,
                     const GpuPreferences& gpu_preferences)
    : task_runner_(std::move(task_runner)),
      sync_point_manager_(sync_point_manager),
      blocked_time_collection_enabled_(
          gpu_preferences.enable_gpu_blocked_time_metric) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Store weak ptr separately because calling GetWeakPtr() is not thread safe.
  weak_ptr_ = weak_factory_.GetWeakPtr();

  if (blocked_time_collection_enabled_ && !base::ThreadTicks::IsSupported()) {
    DLOG(ERROR) << "GPU Blocked time collection is enabled but not supported.";
  }
}

Scheduler::~Scheduler() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

SequenceId Scheduler::CreateSequence(SchedulingPriority priority) {
  base::AutoLock auto_lock(lock_);
  scoped_refptr<SyncPointOrderData> order_data =
      sync_point_manager_->CreateSyncPointOrderData();
  SequenceId sequence_id = order_data->sequence_id();
  auto sequence = std::make_unique<Sequence>(this, sequence_id, priority,
                                             std::move(order_data));
  sequences_.emplace(sequence_id, std::move(sequence));
  return sequence_id;
}

void Scheduler::DestroySequence(SequenceId sequence_id) {
  base::circular_deque<Sequence::Task> tasks_to_be_destroyed;
  {
    base::AutoLock auto_lock(lock_);

    Sequence* sequence = GetSequence(sequence_id);
    DCHECK(sequence);
    if (sequence->scheduled())
      rebuild_scheduling_queue_ = true;

    tasks_to_be_destroyed = std::move(sequence->tasks_);
    sequences_.erase(sequence_id);
  }
}

Scheduler::Sequence* Scheduler::GetSequence(SequenceId sequence_id) {
  lock_.AssertAcquired();
  auto it = sequences_.find(sequence_id);
  if (it != sequences_.end())
    return it->second.get();
  return nullptr;
}

void Scheduler::EnableSequence(SequenceId sequence_id) {
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  sequence->SetEnabled(true);
}

void Scheduler::DisableSequence(SequenceId sequence_id) {
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  sequence->SetEnabled(false);
}

void Scheduler::RaisePriorityForClientWait(SequenceId sequence_id,
                                           CommandBufferId command_buffer_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  sequence->AddClientWait(command_buffer_id);
}

void Scheduler::ResetPriorityForClientWait(SequenceId sequence_id,
                                           CommandBufferId command_buffer_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  sequence->RemoveClientWait(command_buffer_id);
}

void Scheduler::ScheduleTask(Task task) {
  base::AutoLock auto_lock(lock_);
  ScheduleTaskHelper(std::move(task));
}

void Scheduler::ScheduleTasks(std::vector<Task> tasks) {
  base::AutoLock auto_lock(lock_);
  for (auto& task : tasks)
    ScheduleTaskHelper(std::move(task));
}

void Scheduler::ScheduleTaskHelper(Task task) {
  lock_.AssertAcquired();
  SequenceId sequence_id = task.sequence_id;
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);

  uint32_t order_num = sequence->ScheduleTask(std::move(task.closure));

  for (const SyncToken& sync_token : task.sync_token_fences) {
    SequenceId release_sequence_id =
        sync_point_manager_->GetSyncTokenReleaseSequenceId(sync_token);
    if (sync_point_manager_->WaitNonThreadSafe(
            sync_token, sequence_id, order_num, task_runner_,
            base::BindOnce(&Scheduler::SyncTokenFenceReleased, weak_ptr_,
                           sync_token, order_num, release_sequence_id,
                           sequence_id))) {
      sequence->AddWaitFence(sync_token, order_num, release_sequence_id);
    }
  }

  TryScheduleSequence(sequence);
}

void Scheduler::ContinueTask(SequenceId sequence_id,
                             base::OnceClosure closure) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  sequence->ContinueTask(std::move(closure));
}

bool Scheduler::ShouldYield(SequenceId sequence_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);

  RebuildSchedulingQueue();

  if (scheduling_queue_.empty())
    return false;

  Sequence* running_sequence = GetSequence(sequence_id);
  DCHECK(running_sequence);
  DCHECK(running_sequence->running());

  Sequence* next_sequence = GetSequence(scheduling_queue_.front().sequence_id);
  DCHECK(next_sequence);
  DCHECK(next_sequence->scheduled());

  return running_sequence->ShouldYieldTo(next_sequence);
}

base::WeakPtr<Scheduler> Scheduler::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void Scheduler::SyncTokenFenceReleased(const SyncToken& sync_token,
                                       uint32_t order_num,
                                       SequenceId release_sequence_id,
                                       SequenceId waiting_sequence_id) {
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(waiting_sequence_id);

  if (sequence)
    sequence->RemoveWaitFence(sync_token, order_num, release_sequence_id);
}

void Scheduler::TryScheduleSequence(Sequence* sequence) {
  lock_.AssertAcquired();

  if (sequence->running()) {
    // Update priority of running sequence because of sync token releases.
    DCHECK(running_);
    sequence->UpdateRunningPriority();
  } else if (sequence->NeedsRescheduling()) {
    // Rebuild scheduling queue if priority changed for a scheduled sequence.
    DCHECK(running_);
    DCHECK(sequence->IsRunnable());
    rebuild_scheduling_queue_ = true;
  } else if (!sequence->scheduled() && sequence->IsRunnable()) {
    // Insert into scheduling queue if sequence isn't already scheduled.
    SchedulingState scheduling_state = sequence->SetScheduled();
    scheduling_queue_.push_back(scheduling_state);
    std::push_heap(scheduling_queue_.begin(), scheduling_queue_.end(),
                   &SchedulingState::Comparator);
    if (!running_) {
      TRACE_EVENT_ASYNC_BEGIN0("gpu", "Scheduler::Running", this);
      running_ = true;
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&Scheduler::RunNextTask, weak_ptr_));
    }
  }
}

void Scheduler::RebuildSchedulingQueue() {
  DCHECK(thread_checker_.CalledOnValidThread());
  lock_.AssertAcquired();

  if (!rebuild_scheduling_queue_)
    return;
  rebuild_scheduling_queue_ = false;

  scheduling_queue_.clear();
  for (const auto& kv : sequences_) {
    Sequence* sequence = kv.second.get();
    if (!sequence->IsRunnable() || sequence->running())
      continue;
    SchedulingState scheduling_state = sequence->SetScheduled();
    scheduling_queue_.push_back(scheduling_state);
  }

  std::make_heap(scheduling_queue_.begin(), scheduling_queue_.end(),
                 &SchedulingState::Comparator);
}

void Scheduler::RunNextTask() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);

  RebuildSchedulingQueue();

  if (scheduling_queue_.empty()) {
    TRACE_EVENT_ASYNC_END0("gpu", "Scheduler::Running", this);
    running_ = false;
    return;
  }

  std::pop_heap(scheduling_queue_.begin(), scheduling_queue_.end(),
                &SchedulingState::Comparator);
  SchedulingState state = scheduling_queue_.back();
  scheduling_queue_.pop_back();

  base::ElapsedTimer task_timer;

  Sequence* sequence = GetSequence(state.sequence_id);
  DCHECK(sequence);

  base::OnceClosure closure;
  uint32_t order_num = sequence->BeginTask(&closure);
  DCHECK_EQ(order_num, state.order_num);

  TRACE_EVENT_WITH_FLOW1("gpu,toplevel.flow", "Scheduler::RunNextTask",
                         GetTaskFlowId(state.sequence_id.value(), order_num),
                         TRACE_EVENT_FLAG_FLOW_IN, "state", state.AsValue());

  // Begin/FinishProcessingOrderNumber must be called with the lock released
  // because they can renter the scheduler in Enable/DisableSequence.
  scoped_refptr<SyncPointOrderData> order_data = sequence->order_data();
  {
    base::AutoUnlock auto_unlock(lock_);
    order_data->BeginProcessingOrderNumber(order_num);

    if (blocked_time_collection_enabled_ && base::ThreadTicks::IsSupported()) {
      // We can't call base::ThreadTicks::Now() if it's not supported
      base::ThreadTicks thread_time_start = base::ThreadTicks::Now();
      base::TimeTicks wall_time_start = base::TimeTicks::Now();

      std::move(closure).Run();

      base::TimeDelta thread_time_elapsed =
          base::ThreadTicks::Now() - thread_time_start;
      base::TimeDelta wall_time_elapsed =
          base::TimeTicks::Now() - wall_time_start;
      base::TimeDelta blocked_time = wall_time_elapsed - thread_time_elapsed;

      total_blocked_time_ += blocked_time;
    } else {
      std::move(closure).Run();
    }

    if (order_data->IsProcessingOrderNumber())
      order_data->FinishProcessingOrderNumber(order_num);
  }

  // Check if sequence hasn't been destroyed.
  sequence = GetSequence(state.sequence_id);
  if (sequence) {
    sequence->FinishTask();
    if (sequence->IsRunnable()) {
      SchedulingState scheduling_state = sequence->SetScheduled();
      scheduling_queue_.push_back(scheduling_state);
      std::push_heap(scheduling_queue_.begin(), scheduling_queue_.end(),
                     &SchedulingState::Comparator);
    }
  }

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "GPU.Scheduler.RunTaskTime", task_timer.Elapsed(),
      base::TimeDelta::FromMicroseconds(10), base::TimeDelta::FromSeconds(30),
      100);

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&Scheduler::RunNextTask, weak_ptr_));
}

base::TimeDelta Scheduler::TakeTotalBlockingTime() {
  if (!blocked_time_collection_enabled_ || !base::ThreadTicks::IsSupported())
    return base::TimeDelta::Min();
  base::TimeDelta result;
  std::swap(result, total_blocked_time_);
  return result;
}

}  // namespace gpu
