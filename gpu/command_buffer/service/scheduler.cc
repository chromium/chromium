// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/scheduler.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/hash/md5_constexpr.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "gpu/command_buffer/service/scheduler_dfs.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

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
                      std::vector<SyncToken> sync_token_fences,
                      ReportingCallback report_callback)
    : sequence_id(sequence_id),
      closure(std::move(closure)),
      sync_token_fences(std::move(sync_token_fences)),
      report_callback(std::move(report_callback)) {}
Scheduler::Task::Task(Task&& other) = default;
Scheduler::Task::~Task() = default;
Scheduler::Task& Scheduler::Task::operator=(Task&& other) = default;

Scheduler::SchedulingState::SchedulingState() = default;
Scheduler::SchedulingState::SchedulingState(const SchedulingState& other) =
    default;
Scheduler::SchedulingState::~SchedulingState() = default;

Scheduler::ScopedAddWaitingPriority::ScopedAddWaitingPriority(
    Scheduler* scheduler,
    SequenceId sequence_id,
    SchedulingPriority priority)
    : scheduler_(scheduler), sequence_id_(sequence_id), priority_(priority) {
  if (auto& scheduler_dfs = scheduler_->scheduler_dfs_) {
    // Similar to RaisePriorityForClientWait, the new scheduler explicitly
    // relies on SetSequencePriority. Remove ScopedAddWaitingPriority once the
    // old scheduler is removed.
    scheduler_dfs->SetSequencePriority(sequence_id, priority);
  } else {
    scheduler_->AddWaitingPriority(sequence_id_, priority_);
  }
}
Scheduler::ScopedAddWaitingPriority::~ScopedAddWaitingPriority() {
  if (auto& scheduler_dfs = scheduler_->scheduler_dfs_) {
    // See comment in constructor.
    scheduler_dfs->SetSequencePriority(
        sequence_id_, scheduler_dfs->GetSequenceDefaultPriority(sequence_id_));
  } else {
    scheduler_->RemoveWaitingPriority(sequence_id_, priority_);
  }
}

void Scheduler::SchedulingState::WriteIntoTrace(
    perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("sequence_id", sequence_id.GetUnsafeValue());
  dict.Add("priority", SchedulingPriorityToString(priority));
  dict.Add("order_num", order_num);
}

Scheduler::Sequence::Task::Task(base::OnceClosure closure,
                                uint32_t order_num,
                                ReportingCallback report_callback)
    : closure(std::move(closure)),
      order_num(order_num),
      report_callback(std::move(report_callback)) {}

Scheduler::Sequence::Task::Task(Task&& other) = default;
Scheduler::Sequence::Task::~Task() {
  DCHECK(report_callback.is_null());
}

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

Scheduler::PerThreadState::PerThreadState() = default;
Scheduler::PerThreadState::PerThreadState(PerThreadState&& other) = default;
Scheduler::PerThreadState::~PerThreadState() = default;
Scheduler::PerThreadState& Scheduler::PerThreadState::operator=(
    PerThreadState&& other) = default;

Scheduler::Sequence::Sequence(
    Scheduler* scheduler,
    SequenceId sequence_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    SchedulingPriority priority,
    scoped_refptr<SyncPointOrderData> order_data)
    : scheduler_(scheduler),
      sequence_id_(sequence_id),
      task_runner_(std::move(task_runner)),
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
  if (task_runner() != other->task_runner())
    return false;
  if (!running() || !other->scheduled())
    return false;
  return other->scheduling_state_.RunsBefore(scheduling_state_);
}

void Scheduler::Sequence::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;
  if (enabled) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("gpu", "SequenceEnabled",
                                      TRACE_ID_LOCAL(this), "sequence_id",
                                      sequence_id_.GetUnsafeValue());
  } else {
    TRACE_EVENT_NESTABLE_ASYNC_END1("gpu", "SequenceEnabled",
                                    TRACE_ID_LOCAL(this), "sequence_id",
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

  tasks_.push_front({std::move(closure), order_num, ReportingCallback()});
  order_data_->PauseProcessingOrderNumber(order_num);
}

uint32_t Scheduler::Sequence::ScheduleTask(base::OnceClosure closure,
                                           ReportingCallback report_callback) {
  uint32_t order_num = order_data_->GenerateUnprocessedOrderNumber();
  TRACE_EVENT_WITH_FLOW0("gpu,toplevel.flow", "Scheduler::ScheduleTask",
                         GetTaskFlowId(sequence_id_.value(), order_num),
                         TRACE_EVENT_FLAG_FLOW_OUT);
  tasks_.push_back({std::move(closure), order_num, std::move(report_callback)});
  return order_num;
}

base::TimeDelta Scheduler::Sequence::FrontTaskWaitingDependencyDelta() {
  DCHECK(!tasks_.empty());
  if (tasks_.front().first_dependency_added.is_null()) {
    // didn't wait for dependencies.
    return base::TimeDelta();
  }
  return tasks_.front().running_ready - tasks_.front().first_dependency_added;
}

base::TimeDelta Scheduler::Sequence::FrontTaskSchedulingDelay() {
  DCHECK(!tasks_.empty());
  return base::TimeTicks::Now() - tasks_.front().running_ready;
}

uint32_t Scheduler::Sequence::BeginTask(base::OnceClosure* closure) {
  DCHECK(closure);
  DCHECK(!tasks_.empty());
  DCHECK_EQ(running_state_, SCHEDULED);

  running_state_ = RUNNING;

  *closure = std::move(tasks_.front().closure);
  uint32_t order_num = tasks_.front().order_num;
  if (!tasks_.front().report_callback.is_null()) {
    std::move(tasks_.front().report_callback).Run(tasks_.front().running_ready);
  }
  tasks_.pop_front();

  return order_num;
}

void Scheduler::Sequence::FinishTask() {
  DCHECK_EQ(running_state_, RUNNING);
  running_state_ = IDLE;
}

void Scheduler::Sequence::SetLastTaskFirstDependencyTimeIfNeeded() {
  DCHECK(!tasks_.empty());
  if (tasks_.back().first_dependency_added.is_null()) {
    // Fence are always added for the last task (which should always exists).
    tasks_.back().first_dependency_added = base::TimeTicks::Now();
  }
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

    for (auto& task : tasks_) {
      if (order_num == task.order_num) {
        // The fence applies to this task, bump the readiness timestamp
        task.running_ready = base::TimeTicks::Now();
        break;
      } else if (order_num < task.order_num) {
        // Updated all task related to this fence.
        break;
      }
    }

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
  TRACE_EVENT2("gpu", "Scheduler::Sequence::AddWaitingPriority", "sequence_id",
               sequence_id_.GetUnsafeValue(), "new_priority",
               SchedulingPriorityToString(priority));

  scheduler_->lock_.AssertAcquired();

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
  scheduler_->lock_.AssertAcquired();
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

Scheduler::Scheduler(SyncPointManager* sync_point_manager,
                     const GpuPreferences& gpu_preferences)
    : sync_point_manager_(sync_point_manager),
      blocked_time_collection_enabled_(
          gpu_preferences.enable_gpu_blocked_time_metric) {
  if (blocked_time_collection_enabled_ && !base::ThreadTicks::IsSupported())
    DLOG(ERROR) << "GPU Blocked time collection is enabled but not supported.";

  if (base::FeatureList::IsEnabled(features::kUseGpuSchedulerDfs)) {
    scheduler_dfs_ =
        std::make_unique<SchedulerDfs>(sync_point_manager, gpu_preferences);
  }
}

Scheduler::~Scheduler() {
  base::AutoLock auto_lock(lock_);

  // Sequences as well as tasks posted to the threads have "this" pointer of the
  // Scheduler. Hence adding DCHECKS to make sure sequences are
  // finished/destroyed and none of the threads are running by the time
  // scheduler is destroyed.
  DCHECK(sequence_map_.empty());
  for (const auto& per_thread_state : per_thread_state_map_)
    DCHECK(!per_thread_state.second.running);
}

SequenceId Scheduler::CreateSequence(
    SchedulingPriority priority,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (scheduler_dfs_) {
    return scheduler_dfs_->CreateSequence(priority, task_runner);
  }
  base::AutoLock auto_lock(lock_);
  scoped_refptr<SyncPointOrderData> order_data =
      sync_point_manager_->CreateSyncPointOrderData();
  SequenceId sequence_id = order_data->sequence_id();
  auto sequence =
      std::make_unique<Sequence>(this, sequence_id, std::move(task_runner),
                                 priority, std::move(order_data));
  sequence_map_.emplace(sequence_id, std::move(sequence));
  return sequence_id;
}

SequenceId Scheduler::CreateSequenceForTesting(SchedulingPriority priority) {
  if (scheduler_dfs_) {
    return scheduler_dfs_->CreateSequenceForTesting(priority);  // IN-TEST
  }
  // This will create the sequence on the thread on which this method is called.
  return CreateSequence(priority,
                        base::SingleThreadTaskRunner::GetCurrentDefault());
}

void Scheduler::DestroySequence(SequenceId sequence_id) {
  if (scheduler_dfs_) {
    return scheduler_dfs_->DestroySequence(sequence_id);
  }

  base::circular_deque<Sequence::Task> tasks_to_be_destroyed;
  {
    base::AutoLock auto_lock(lock_);

    Sequence* sequence = GetSequence(sequence_id);
    DCHECK(sequence);
    if (sequence->scheduled()) {
      per_thread_state_map_[sequence->task_runner()].rebuild_scheduling_queue =
          true;
    }

    tasks_to_be_destroyed = std::move(sequence->tasks_);
    sequence_map_.erase(sequence_id);
  }
}

Scheduler::Sequence* Scheduler::GetSequence(SequenceId sequence_id) {
  DCHECK(!scheduler_dfs_);
  lock_.AssertAcquired();
  auto it = sequence_map_.find(sequence_id);
  if (it != sequence_map_.end())
    return it->second.get();
  return nullptr;
}

void Scheduler::EnableSequence(SequenceId sequence_id) {
  if (scheduler_dfs_) {
    return scheduler_dfs_->EnableSequence(sequence_id);
  }
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  sequence->SetEnabled(true);
}

void Scheduler::DisableSequence(SequenceId sequence_id) {
  if (scheduler_dfs_) {
    return scheduler_dfs_->DisableSequence(sequence_id);
  }
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  sequence->SetEnabled(false);
}

void Scheduler::RaisePriorityForClientWait(SequenceId sequence_id,
                                           CommandBufferId command_buffer_id) {
  // SchedulerDfs does not have Raise/ResetPriorityForClientWait, and instead
  // relies on explicitly setting sequence priority. After this scheduler is
  // completely replaced by SchedulerDfs, these functions should be removed and
  // the implementation switched at the client call-site.
  if (scheduler_dfs_) {
    return scheduler_dfs_->SetSequencePriority(sequence_id,
                                               SchedulingPriority::kHigh);
  }
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  sequence->AddClientWait(command_buffer_id);
}

void Scheduler::ResetPriorityForClientWait(SequenceId sequence_id,
                                           CommandBufferId command_buffer_id) {
  // See comment in RaisePriorityForClientWait.
  if (scheduler_dfs_) {
    return scheduler_dfs_->SetSequencePriority(
        sequence_id, scheduler_dfs_->GetSequenceDefaultPriority(sequence_id));
  }
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  sequence->RemoveClientWait(command_buffer_id);
}

void Scheduler::ScheduleTask(Task task) {
  if (scheduler_dfs_)
    return scheduler_dfs_->ScheduleTask(std::move(task));

  base::AutoLock auto_lock(lock_);
  ScheduleTaskHelper(std::move(task));
}

void Scheduler::ScheduleTasks(std::vector<Task> tasks) {
  if (scheduler_dfs_)
    return scheduler_dfs_->ScheduleTasks(std::move(tasks));

  base::AutoLock auto_lock(lock_);
  for (auto& task : tasks)
    ScheduleTaskHelper(std::move(task));
}

void Scheduler::ScheduleTaskHelper(Task task) {
  lock_.AssertAcquired();
  SequenceId sequence_id = task.sequence_id;
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);

  auto* task_runner = sequence->task_runner();
  uint32_t order_num = sequence->ScheduleTask(std::move(task.closure),
                                              std::move(task.report_callback));

  for (const SyncToken& sync_token : ReduceSyncTokens(task.sync_token_fences)) {
    SequenceId release_sequence_id =
        sync_point_manager_->GetSyncTokenReleaseSequenceId(sync_token);
    // base::Unretained is safe here since all sequences and corresponding sync
    // point callbacks will be released before the scheduler is destroyed (even
    // though sync point manager itself outlives the scheduler briefly).
    if (sync_point_manager_->WaitNonThreadSafe(
            sync_token, sequence_id, order_num, task_runner,
            base::BindOnce(&Scheduler::SyncTokenFenceReleased,
                           base::Unretained(this), sync_token, order_num,
                           release_sequence_id, sequence_id))) {
      sequence->AddWaitFence(sync_token, order_num, release_sequence_id);
      sequence->SetLastTaskFirstDependencyTimeIfNeeded();
    }
  }

  TryScheduleSequence(sequence);
}

void Scheduler::ContinueTask(SequenceId sequence_id,
                             base::OnceClosure closure) {
  if (scheduler_dfs_)
    return scheduler_dfs_->ContinueTask(sequence_id, std::move(closure));
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  DCHECK(sequence->task_runner()->BelongsToCurrentThread());
  sequence->ContinueTask(std::move(closure));
}

bool Scheduler::ShouldYield(SequenceId sequence_id) {
  if (scheduler_dfs_)
    return scheduler_dfs_->ShouldYield(sequence_id);

  base::AutoLock auto_lock(lock_);

  Sequence* running_sequence = GetSequence(sequence_id);
  DCHECK(running_sequence);
  DCHECK(running_sequence->running());
  DCHECK(running_sequence->task_runner()->BelongsToCurrentThread());

  const auto& scheduling_queue =
      RebuildSchedulingQueueIfNeeded(running_sequence->task_runner());

  if (scheduling_queue.empty())
    return false;

  Sequence* next_sequence = GetSequence(scheduling_queue.front().sequence_id);
  DCHECK(next_sequence);
  DCHECK(next_sequence->scheduled());

  return running_sequence->ShouldYieldTo(next_sequence);
}

void Scheduler::AddWaitingPriority(SequenceId sequence_id,
                                   SchedulingPriority priority) {
  DCHECK(!scheduler_dfs_);
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(sequence_id);
  if (sequence)
    sequence->AddWaitingPriority(priority);
}

void Scheduler::RemoveWaitingPriority(SequenceId sequence_id,
                                      SchedulingPriority priority) {
  DCHECK(!scheduler_dfs_);
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(sequence_id);
  if (sequence)
    sequence->RemoveWaitingPriority(priority);
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

  auto* task_runner = sequence->task_runner();
  auto& thread_state = per_thread_state_map_[task_runner];

  if (sequence->running()) {
    // Update priority of running sequence because of sync token releases.
    DCHECK(thread_state.running);
    sequence->UpdateRunningPriority();
  } else if (sequence->NeedsRescheduling()) {
    // Rebuild scheduling queue if priority changed for a scheduled sequence.
    DCHECK(thread_state.running);
    DCHECK(sequence->IsRunnable());
    per_thread_state_map_[task_runner].rebuild_scheduling_queue = true;
  } else if (!sequence->scheduled() && sequence->IsRunnable()) {
    // Insert into scheduling queue if sequence isn't already scheduled.
    SchedulingState scheduling_state = sequence->SetScheduled();
    auto& scheduling_queue =
        per_thread_state_map_[task_runner].scheduling_queue;
    scheduling_queue.push_back(scheduling_state);
    std::push_heap(scheduling_queue.begin(), scheduling_queue.end(),
                   &SchedulingState::Comparator);
    if (!thread_state.running) {
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("gpu", "Scheduler::Running",
                                        TRACE_ID_LOCAL(this));
      thread_state.running = true;
      thread_state.run_next_task_scheduled = base::TimeTicks::Now();
      task_runner->PostTask(FROM_HERE, base::BindOnce(&Scheduler::RunNextTask,
                                                      base::Unretained(this)));
    }
  }
}

std::vector<Scheduler::SchedulingState>&
Scheduler::RebuildSchedulingQueueIfNeeded(
    base::SingleThreadTaskRunner* task_runner) {
  lock_.AssertAcquired();

  auto& thread_state = per_thread_state_map_[task_runner];
  auto& scheduling_queue = thread_state.scheduling_queue;

  if (!thread_state.rebuild_scheduling_queue)
    return scheduling_queue;
  thread_state.rebuild_scheduling_queue = false;

  scheduling_queue.clear();
  for (const auto& kv : sequence_map_) {
    Sequence* sequence = kv.second.get();
    if (!sequence->IsRunnable() || sequence->running() ||
        sequence->task_runner() != task_runner) {
      continue;
    }
    SchedulingState scheduling_state = sequence->SetScheduled();
    scheduling_queue.push_back(scheduling_state);
  }

  std::make_heap(scheduling_queue.begin(), scheduling_queue.end(),
                 &SchedulingState::Comparator);
  return scheduling_queue;
}

void Scheduler::RunNextTask() {
  base::AutoLock auto_lock(lock_);
  auto* task_runner = base::SingleThreadTaskRunner::GetCurrentDefault().get();
  auto* thread_state = &per_thread_state_map_[task_runner];

  // Subsampling these metrics reduced CPU utilization (crbug.com/1295441).
  const bool log_histograms = metrics_subsampler_.ShouldSample(0.001);

  if (log_histograms) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "GPU.Scheduler.ThreadSuspendedTime",
        base::TimeTicks::Now() - thread_state->run_next_task_scheduled,
        base::Microseconds(10), base::Seconds(30), 100);
  }

  SchedulingState state;
  {
    auto& scheduling_queue = RebuildSchedulingQueueIfNeeded(task_runner);
    if (scheduling_queue.empty()) {
      TRACE_EVENT_NESTABLE_ASYNC_END0("gpu", "Scheduler::Running",
                                      TRACE_ID_LOCAL(this));
      thread_state->running = false;
      return;
    }

    state = scheduling_queue.front();
    std::pop_heap(scheduling_queue.begin(), scheduling_queue.end(),
                  &SchedulingState::Comparator);
    scheduling_queue.pop_back();
  }

  Sequence* sequence = GetSequence(state.sequence_id);
  DCHECK(sequence);
  DCHECK_EQ(sequence->task_runner(), task_runner);

  if (log_histograms) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "GPU.Scheduler.TaskDependencyTime",
        sequence->FrontTaskWaitingDependencyDelta(), base::Microseconds(10),
        base::Seconds(30), 100);

    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "GPU.Scheduler.TaskSchedulingDelayTime",
        sequence->FrontTaskSchedulingDelay(), base::Microseconds(10),
        base::Seconds(30), 100);
  }

  base::OnceClosure closure;
  uint32_t order_num = sequence->BeginTask(&closure);
  DCHECK_EQ(order_num, state.order_num);

  TRACE_EVENT_WITH_FLOW1("gpu,toplevel.flow", "Scheduler::RunNextTask",
                         GetTaskFlowId(state.sequence_id.value(), order_num),
                         TRACE_EVENT_FLAG_FLOW_IN, "state", state);

  // Begin/FinishProcessingOrderNumber must be called with the lock released
  // because they can renter the scheduler in Enable/DisableSequence.
  scoped_refptr<SyncPointOrderData> order_data = sequence->order_data();

  // Unset pointers before releasing the lock to prevent accidental data race.
  thread_state = nullptr;
  sequence = nullptr;

  base::TimeDelta blocked_time;
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

      blocked_time += (wall_time_elapsed - thread_time_elapsed);
    } else {
      std::move(closure).Run();
    }

    if (order_data->IsProcessingOrderNumber())
      order_data->FinishProcessingOrderNumber(order_num);
  }

  total_blocked_time_ += blocked_time;

  // Reset pointers after reaquiring the lock.
  thread_state = &per_thread_state_map_[task_runner];
  sequence = GetSequence(state.sequence_id);

  // Check if sequence hasn't been destroyed.
  if (sequence) {
    sequence->FinishTask();
    if (sequence->IsRunnable()) {
      auto& scheduling_queue = thread_state->scheduling_queue;

      SchedulingState scheduling_state = sequence->SetScheduled();
      scheduling_queue.push_back(scheduling_state);
      std::push_heap(scheduling_queue.begin(), scheduling_queue.end(),
                     &SchedulingState::Comparator);
    }
  }

  // Avoid scheduling another RunNextTask if we're done with all tasks.
  auto& scheduling_queue = RebuildSchedulingQueueIfNeeded(task_runner);
  if (scheduling_queue.empty()) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("gpu", "Scheduler::Running",
                                    TRACE_ID_LOCAL(this));
    thread_state->running = false;
    return;
  }

  thread_state->run_next_task_scheduled = base::TimeTicks::Now();
  task_runner->PostTask(FROM_HERE, base::BindOnce(&Scheduler::RunNextTask,
                                                  base::Unretained(this)));
}

base::TimeDelta Scheduler::TakeTotalBlockingTime() {
  if (scheduler_dfs_)
    return scheduler_dfs_->TakeTotalBlockingTime();

  if (!blocked_time_collection_enabled_ || !base::ThreadTicks::IsSupported())
    return base::TimeDelta::Min();
  base::AutoLock auto_lock(lock_);
  base::TimeDelta result;
  std::swap(result, total_blocked_time_);
  return result;
}

base::SingleThreadTaskRunner* Scheduler::GetTaskRunnerForTesting(
    SequenceId sequence_id) {
  if (scheduler_dfs_) {
    return scheduler_dfs_->GetTaskRunnerForTesting(sequence_id);  // IN-TEST
  }
  base::AutoLock auto_lock(lock_);
  return GetSequence(sequence_id)->task_runner();
}

}  // namespace gpu
