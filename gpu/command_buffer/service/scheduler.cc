// Copyright 2022 The Chromium Authors
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
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
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
                      TaskCallback task_callback,
                      std::vector<SyncToken> sync_token_fences,
                      const SyncToken& release,
                      ReportingCallback report_callback)
    : sequence_id(sequence_id),
      task_callback(std::move(task_callback)),
      sync_token_fences(std::move(sync_token_fences)),
      release(release),
      report_callback(std::move(report_callback)) {}

Scheduler::Task::Task(SequenceId sequence_id,
                      base::OnceClosure task_closure,
                      std::vector<SyncToken> sync_token_fences,
                      const SyncToken& release,
                      ReportingCallback report_callback)
    : sequence_id(sequence_id),
      task_closure(std::move(task_closure)),
      sync_token_fences(std::move(sync_token_fences)),
      release(release),
      report_callback(std::move(report_callback)) {}

Scheduler::Task::Task(SequenceId sequence_id,
                      base::OnceClosure task_closure,
                      std::vector<SyncToken> sync_token_fences,
                      ReportingCallback report_callback)
    : Task(sequence_id,
           std::move(task_closure),
           std::move(sync_token_fences),
           /*release=*/{},
           std::move(report_callback)) {}

Scheduler::Task::Task(Task&& other) = default;
Scheduler::Task::~Task() = default;
Scheduler::Task& Scheduler::Task::operator=(Task&& other) = default;

Scheduler::ScopedSetSequencePriority::ScopedSetSequencePriority(
    Scheduler* scheduler,
    SequenceId sequence_id,
    SchedulingPriority priority)
    : scheduler_(scheduler), sequence_id_(sequence_id), priority_(priority) {
  scheduler_->SetSequencePriority(sequence_id, priority);
}
Scheduler::ScopedSetSequencePriority::~ScopedSetSequencePriority() {
  scheduler_->SetSequencePriority(
      sequence_id_, scheduler_->GetSequenceDefaultPriority(sequence_id_));
}

Scheduler::SchedulingState::SchedulingState() = default;
Scheduler::SchedulingState::SchedulingState(const SchedulingState& other) =
    default;
Scheduler::SchedulingState::~SchedulingState() = default;

void Scheduler::SchedulingState::WriteIntoTrace(
    perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("sequence_id", sequence_id.GetUnsafeValue());
  dict.Add("priority", SchedulingPriorityToString(priority));
  dict.Add("order_num", order_num);
}

bool Scheduler::SchedulingState::operator==(
    const Scheduler::SchedulingState& rhs) const {
  return std::tie(sequence_id, priority, order_num) ==
         std::tie(rhs.sequence_id, rhs.priority, rhs.order_num);
}

Scheduler::PerThreadState::PerThreadState() = default;
Scheduler::PerThreadState::PerThreadState(PerThreadState&& other) = default;
Scheduler::PerThreadState::~PerThreadState() = default;
Scheduler::PerThreadState& Scheduler::PerThreadState::operator=(
    PerThreadState&& other) = default;

Scheduler::Sequence::Sequence(
    Scheduler* scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    SchedulingPriority priority,
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id)
    : TaskGraph::Sequence(&scheduler->task_graph_,
                          base::BindRepeating(&Sequence::OnFrontTaskUnblocked,
                                              base::Unretained(this)),
                          task_runner,
                          namespace_id,
                          command_buffer_id),
      scheduler_(scheduler),
      task_runner_(std::move(task_runner)),
      default_priority_(priority),
      current_priority_(priority) {}

Scheduler::Sequence::~Sequence() {
  for (const auto& wait_fence : wait_fences_) {
    Sequence* release_sequence =
        scheduler_->GetSequence(wait_fence.release_sequence_id);
    if (release_sequence) {
      scheduler_->TryScheduleSequence(release_sequence);
    }
  }
}

bool Scheduler::Sequence::ShouldYieldTo(const Sequence* other) const {
  if (task_runner() != other->task_runner())
    return false;
  if (!running()) {
    return false;
  }
  return SchedulingState::RunsBefore(other->scheduling_state_,
                                     scheduling_state_);
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
  DCHECK(HasTasksAndEnabled());
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

void Scheduler::Sequence::ContinueTask(base::OnceClosure task_closure) {
  DCHECK_EQ(running_state_, RUNNING);
  TaskGraph::Sequence::ContinueTask(std::move(task_closure));
}

uint32_t Scheduler::Sequence::AddTask(
    base::OnceClosure task_closure,
    std::vector<SyncToken> wait_fences,
    const SyncToken& release,
    TaskGraph::ReportingCallback report_callback) {
  uint32_t order_num = TaskGraph::Sequence::AddTask(
      std::move(task_closure), std::move(wait_fences), release,
      std::move(report_callback));

  TRACE_EVENT_WITH_FLOW0("gpu,toplevel.flow", "Scheduler::ScheduleTask",
                         GetTaskFlowId(sequence_id_.value(), order_num),
                         TRACE_EVENT_FLAG_FLOW_OUT);
  return order_num;
}

uint32_t Scheduler::Sequence::BeginTask(base::OnceClosure* task_closure) {
  DCHECK_EQ(running_state_, SCHEDULED);

  running_state_ = RUNNING;

  return TaskGraph::Sequence::BeginTask(task_closure);
}

void Scheduler::Sequence::FinishTask() {
  DCHECK_EQ(running_state_, RUNNING);
  running_state_ = SCHEDULED;
  TaskGraph::Sequence::FinishTask();
}

void Scheduler::Sequence::OnFrontTaskUnblocked() {
  scheduler_->TryScheduleSequence(this);
}

Scheduler::Scheduler(SyncPointManager* sync_point_manager)
    : task_graph_(sync_point_manager) {}

Scheduler::~Scheduler() {
  base::AutoLock auto_lock(lock());

  // Sequences as well as tasks posted to the threads have "this" pointer of the
  // Scheduler. Hence adding DCHECKS to make sure sequences are
  // finished/destroyed and none of the threads are running by the time
  // scheduler is destroyed.
  DCHECK(scheduler_sequence_map_.empty());
  for (const auto& per_thread_state : per_thread_state_map_)
    DCHECK(!per_thread_state.second.running);
}

SequenceId Scheduler::CreateSequence(
    SchedulingPriority priority,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return CreateSequence(priority, std::move(task_runner),
                        CommandBufferNamespace::INVALID,
                        /*command_buffer_id=*/{});
}

SequenceId Scheduler::CreateSequence(
    SchedulingPriority priority,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id) {
  auto sequence = std::make_unique<Sequence>(
      this, std::move(task_runner), priority, namespace_id, command_buffer_id);
  SequenceId id = sequence->sequence_id();
  Sequence* sequence_ptr = sequence.get();
  task_graph_.AddSequence(std::move(sequence));

  {
    base::AutoLock auto_lock(lock());
    CHECK_EQ(task_graph_.GetSequence(id), sequence_ptr);
    scheduler_sequence_map_.emplace(id, sequence_ptr);
  }
  return id;
}

void Scheduler::DestroySequence(SequenceId sequence_id) {
  {
    base::AutoLock auto_lock(lock());
    scheduler_sequence_map_.erase(sequence_id);
  }

  task_graph_.DestroySequence(sequence_id);
}

ScopedSyncPointClientState Scheduler::CreateSyncPointClientState(
    SequenceId sequence_id,
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id) {
  return task_graph_.CreateSyncPointClientState(sequence_id, namespace_id,
                                                command_buffer_id);
}

Scheduler::Sequence* Scheduler::GetSequence(SequenceId sequence_id) {
  auto it = scheduler_sequence_map_.find(sequence_id);
  if (it != scheduler_sequence_map_.end()) {
    return it->second;
  }
  return nullptr;
}

void Scheduler::EnableSequence(SequenceId sequence_id) {
  base::AutoLock auto_lock(lock());
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  sequence->SetEnabled(true);
}

void Scheduler::DisableSequence(SequenceId sequence_id) {
  base::AutoLock auto_lock(lock());
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  sequence->SetEnabled(false);
}

SchedulingPriority Scheduler::GetSequenceDefaultPriority(
    SequenceId sequence_id) {
  base::AutoLock auto_lock(lock());
  Sequence* sequence = GetSequence(sequence_id);
  if (sequence) {
    return sequence->default_priority_;
  }
  return SchedulingPriority::kNormal;
}

void Scheduler::SetSequencePriority(SequenceId sequence_id,
                                    SchedulingPriority priority) {
  base::AutoLock auto_lock(lock());
  Sequence* sequence = GetSequence(sequence_id);
  if (sequence) {
    sequence->current_priority_ = priority;
  }
}

void Scheduler::ScheduleTask(Task task) {
  base::AutoLock auto_lock(lock());
  ScheduleTaskHelper(std::move(task));
}

void Scheduler::ScheduleTasks(std::vector<Task> tasks) {
  base::AutoLock auto_lock(lock());
  for (auto& task : tasks)
    ScheduleTaskHelper(std::move(task));
}

void Scheduler::ScheduleTaskHelper(Task task) {
  SequenceId sequence_id = task.sequence_id;
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);

  if (task.task_callback) {
    sequence->AddTask(std::move(task.task_callback),
                      std::move(task.sync_token_fences), task.release,
                      std::move(task.report_callback));
  } else {
    sequence->AddTask(std::move(task.task_closure),
                      std::move(task.sync_token_fences), task.release,
                      std::move(task.report_callback));
  }

  TryScheduleSequence(sequence);
}

void Scheduler::ContinueTask(SequenceId sequence_id,
                             TaskCallback task_callback) {
  base::AutoLock auto_lock(lock());
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  DCHECK(sequence->task_runner()->BelongsToCurrentThread());
  sequence->ContinueTask(std::move(task_callback));
}

void Scheduler::ContinueTask(SequenceId sequence_id,
                             base::OnceClosure task_closure) {
  base::AutoLock auto_lock(lock());
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  DCHECK(sequence->task_runner()->BelongsToCurrentThread());
  sequence->ContinueTask(std::move(task_closure));
}

bool Scheduler::ShouldYield(SequenceId sequence_id) {
  base::AutoLock auto_lock(lock());

  Sequence* running_sequence = GetSequence(sequence_id);
  DCHECK(running_sequence);
  DCHECK(running_sequence->running());
  DCHECK(running_sequence->task_runner()->BelongsToCurrentThread());

  // Call FindNextTask to find the sequence that will run next. This can
  // potentially return nullptr if the only dependency on this thread is a
  // sequence tied to another thread.
  // TODO(elgarawany): Remove ShouldYield entirely and make CommandBufferStub,
  // the only user of ShouldYield, always pause, and leave the scheduling
  // decision to the scheduler.
  Sequence* next_sequence = FindNextTask();
  if (next_sequence == nullptr) {
    return false;
  }

  return running_sequence->ShouldYieldTo(next_sequence);
}

base::SingleThreadTaskRunner* Scheduler::GetTaskRunnerForTesting(
    SequenceId sequence_id) {
  base::AutoLock auto_lock(lock());
  return GetSequence(sequence_id)->task_runner();
}

void Scheduler::TryScheduleSequence(Sequence* sequence) {
  auto* task_runner = sequence->task_runner();
  auto& thread_state = per_thread_state_map_[task_runner];

  DVLOG(10) << "Trying to schedule or wake up sequence "
            << sequence->sequence_id().value()
            << ". running: " << sequence->running() << ".";

  if (sequence->running()) {
    // Update priority of running sequence because of sync token releases.
    DCHECK(thread_state.running);
    sequence->UpdateRunningPriority();
  } else {
    // Insert into scheduling queue if sequence isn't already scheduled.
    if (!sequence->scheduled() && sequence->HasTasksAndEnabled()) {
      sequence->SetScheduled();
    }
    // Wake up RunNextTask if the sequence has work to do. (If the thread is not
    // running, that means that all other sequences were either empty, or
    // waiting for work to be done on another thread).
    if (!thread_state.running && HasAnyUnblockedTasksOnRunner(task_runner)) {
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("gpu", "Scheduler::Running",
                                        TRACE_ID_LOCAL(this));
      DVLOG(10) << "Waking up thread because there is work to do.";
      thread_state.running = true;
      thread_state.run_next_task_scheduled = base::TimeTicks::Now();
      task_runner->PostTask(FROM_HERE, base::BindOnce(&Scheduler::RunNextTask,
                                                      base::Unretained(this)));
    }
  }
}

const std::vector<Scheduler::SchedulingState>&
Scheduler::GetSortedRunnableSequences(
    base::SingleThreadTaskRunner* task_runner) {
  auto& thread_state = per_thread_state_map_[task_runner];
  std::vector<SchedulingState>& sorted_sequences =
      thread_state.sorted_sequences;

  sorted_sequences.clear();
  for (const auto& kv : scheduler_sequence_map_) {
    Sequence* sequence = kv.second;
    // Add any sequence that is enabled, not already running, and has any tasks.
    if (sequence->IsRunnable()) {
      SchedulingState scheduling_state = sequence->SetScheduled();
      sorted_sequences.push_back(scheduling_state);
    }
  }

  // Sort the sequence. We never have more than a few handful of sequences - so
  // this is pretty cheap to do.
  std::stable_sort(sorted_sequences.begin(), sorted_sequences.end(),
                   &SchedulingState::RunsBefore);
  return sorted_sequences;
}

bool Scheduler::HasAnyUnblockedTasksOnRunner(
    const base::SingleThreadTaskRunner* task_runner) const {
  // Loop over all sequences and check if any of them are unblocked and belong
  // to |task_runner|.
  for (const auto& [_, sequence] : scheduler_sequence_map_) {
    CHECK(sequence);
    if (sequence->task_runner() == task_runner && sequence->enabled() &&
        sequence->IsFrontTaskUnblocked()) {
      return true;
    }
  }
  // Either we don't have any enabled sequences, or they are all blocked (this
  // can happen if DrDC is enabled).
  return false;
}

Scheduler::Sequence* Scheduler::FindNextTaskFromRoot(Sequence* root_sequence) {
  if (!root_sequence) {
    return nullptr;
  }

  VLOG_IF(10, !root_sequence->enabled())
      << "Sequence " << root_sequence->sequence_id() << " is not enabled!";
  DVLOG_IF(10, !root_sequence->HasTasks())
      << "Sequence " << root_sequence->sequence_id()
      << " does not have any tasks!";

  // Don't bother looking at disabled sequence, sequences that don't have tasks,
  // and (leaf) sequences that are already running. We don't look at running
  // sequences because their order number is updated *before* they finish, which
  // can make dependencies appear circular.
  if (!root_sequence->IsRunnable()) {
    return nullptr;
  }

  // First, recurse into any dependency that needs to run before the first
  // task in |root_sequence|. The dependencies are sorted by their order num
  // (because of WaitFence ordering).
  const uint32_t first_task_order_num = root_sequence->tasks_.front().order_num;
  DVLOG(10) << "Sequence " << root_sequence->sequence_id()
            << " (order_num: " << first_task_order_num << ") has "
            << root_sequence->wait_fences_.size() << " waits.";

  for (auto fence_iter = root_sequence->wait_fences_.begin();
       fence_iter != root_sequence->wait_fences_.end() &&
       fence_iter->order_num <= first_task_order_num;
       ++fence_iter) {
    // Recurse into the dependent sequence. If a subtask was found, then
    // we're done.
    DVLOG(10) << "Recursing into dependency in sequence "
              << fence_iter->release_sequence_id
              << " (order_num: " << fence_iter->order_num << ").";
    Sequence* release_sequence = GetSequence(fence_iter->release_sequence_id);
    // ShouldYield might be calling this function, and a dependency might depend
    // on the calling sequence, which might have not released its fences yet.
    if (release_sequence && release_sequence->HasTasksAndEnabled() &&
        release_sequence->tasks_.front().order_num >= fence_iter->order_num) {
      continue;
    }
    if (Sequence* result = FindNextTaskFromRoot(release_sequence);
        result != nullptr) {
      return result;
    }
  }
  // It's possible that none of root_sequence's dependencies can be run
  // because they are tied to another thread.
  const bool are_dependencies_done =
      root_sequence->wait_fences_.empty() ||
      root_sequence->wait_fences_.begin()->order_num > first_task_order_num;
  // Return |root_sequence| only if its dependencies are done, and if it can
  // run on the current thread.
  DVLOG_IF(10, root_sequence->task_runner() !=
                   base::SingleThreadTaskRunner::GetCurrentDefault().get())
      << "Will not run sequence because it does not belong to this thread.";
  if (are_dependencies_done &&
      root_sequence->task_runner() ==
          base::SingleThreadTaskRunner::GetCurrentDefault().get()) {
    return root_sequence;
  } else {
    DVLOG_IF(10, !are_dependencies_done)
        << "Sequence " << root_sequence->sequence_id()
        << "'s dependencies are not yet done.";
    return nullptr;
  }
}

Scheduler::Sequence* Scheduler::FindNextTask() {
  auto* task_runner = base::SingleThreadTaskRunner::GetCurrentDefault().get();
  auto& sorted_sequences = GetSortedRunnableSequences(task_runner);
  // Walk the scheduling queue starting with the highest priority sequence and
  // find the first sequence that can be run. The loop will iterate more than
  // once only if DrDC is enabled and the first sequence contains a single
  // dependency tied to another thread.
  for (const SchedulingState& state : sorted_sequences) {
    Sequence* root_sequence = GetSequence(state.sequence_id);
    DVLOG(10) << "FindNextTask: Calling FindNextTaskFromRoot on sequence "
              << root_sequence->sequence_id().value();
    if (Sequence* sequence = FindNextTaskFromRoot(root_sequence);
        sequence != nullptr) {
      return sequence;
    }
  }

  return nullptr;
}

// See comments in scheduler.h for a high-level overview of the algorithm.
void Scheduler::RunNextTask() {
  SequenceId sequence_id;
  DCHECK(sequence_id.is_null());

  {
    base::AutoLock auto_lock(lock());
    auto* task_runner = base::SingleThreadTaskRunner::GetCurrentDefault().get();
    auto* thread_state = &per_thread_state_map_[task_runner];
    DVLOG(10) << "RunNextTask: Task runner is " << (uint64_t)task_runner;

    // Walk the job graph starting from the highest priority roots to find a
    // task to run.
    Sequence* sequence = FindNextTask();

    if (sequence == nullptr) {
      // If there is no sequence to run, it should mean that there are no
      // runnable sequences.
      // TODO(elgarawany): We shouldn't have run RunNextTask if there were no
      // runnable sequences. Change logic to check for that too (that changes
      // old behavior - so leaving for now).

      // TODO(crbug.com/40278526): this assert is firing frequently on
      // Release builds with dcheck_always_on on Intel Macs. It looks
      // like it happens when the browser drops frames.
      /*
      DCHECK(GetSortedRunnableSequences(task_runner).empty())
          << "RunNextTask should not have been called "
             "if it did not have any unblocked tasks.";
      */

      TRACE_EVENT_NESTABLE_ASYNC_END0("gpu", "Scheduler::Running",
                                      TRACE_ID_LOCAL(this));

      DVLOG(10) << "Empty scheduling queue. Sleeping.";
      thread_state->running = false;
      return;
    }

    DCHECK(sequence->task_runner() == task_runner)
        << "FindNextTaskFromRoot returned sequence that does not belong to "
           "this thread.";
    sequence_id = sequence->sequence_id();
  }

  // Now, execute the sequence's task.
  ExecuteSequence(sequence_id);

  // Finally, reschedule RunNextTask if there is any potential remaining work.
  {
    base::AutoLock auto_lock(lock());
    auto* task_runner = base::SingleThreadTaskRunner::GetCurrentDefault().get();
    auto* thread_state = &per_thread_state_map_[task_runner];

    if (!HasAnyUnblockedTasksOnRunner(task_runner)) {
      TRACE_EVENT_NESTABLE_ASYNC_END0("gpu", "Scheduler::Running",
                                      TRACE_ID_LOCAL(this));
      DVLOG(10) << "Thread has no runnable sequences. Sleeping.";
      thread_state->running = false;
      return;
    }
    thread_state->run_next_task_scheduled = base::TimeTicks::Now();
    task_runner->PostTask(FROM_HERE, base::BindOnce(&Scheduler::RunNextTask,
                                                    base::Unretained(this)));
  }
}

void Scheduler::ExecuteSequence(const SequenceId sequence_id) {
  base::AutoLock auto_lock(lock());
  auto* task_runner = base::SingleThreadTaskRunner::GetCurrentDefault().get();
  auto* thread_state = &per_thread_state_map_[task_runner];

  // Subsampling these metrics reduced CPU utilization (crbug.com/1295441).
  const bool log_histograms = metrics_subsampler_.ShouldSample(0.001);

  if (log_histograms) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "GPU.SchedulerDfs.ThreadSuspendedTime",
        base::TimeTicks::Now() - thread_state->run_next_task_scheduled,
        base::Microseconds(10), base::Seconds(30), 100);
  }

  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  DCHECK(sequence->HasTasksAndEnabled());
  DCHECK_EQ(sequence->task_runner(), task_runner);

  DVLOG(10) << "Executing sequence " << sequence_id.value() << ".";

  if (log_histograms) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "GPU.SchedulerDfs.TaskDependencyTime",
        sequence->FrontTaskWaitingDependencyDelta(), base::Microseconds(10),
        base::Seconds(30), 100);

    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "GPU.SchedulerDfs.TaskSchedulingDelayTime",
        sequence->FrontTaskSchedulingDelay(), base::Microseconds(10),
        base::Seconds(30), 100);
  }

  base::OnceClosure task_closure;
  uint32_t order_num = sequence->BeginTask(&task_closure);
  SyncToken release = sequence->current_task_release();

  TRACE_EVENT_WITH_FLOW0("gpu,toplevel.flow", "Scheduler::RunNextTask",
                         GetTaskFlowId(sequence_id.value(), order_num),
                         TRACE_EVENT_FLAG_FLOW_IN);

  // Begin/FinishProcessingOrderNumber must be called with the lock released
  // because they can renter the scheduler in Enable/DisableSequence.
  scoped_refptr<SyncPointOrderData> order_data = sequence->order_data();

  // Unset pointers before releasing the lock to prevent accidental data race.
  thread_state = nullptr;
  sequence = nullptr;

  base::TimeDelta blocked_time;
  {
    base::AutoUnlock auto_unlock(lock());
    order_data->BeginProcessingOrderNumber(order_num);

    std::move(task_closure).Run();

    if (order_data->IsProcessingOrderNumber()) {
      if (release.HasData()) {
        task_graph_.sync_point_manager()->EnsureFenceSyncReleased(
            release, ReleaseCause::kTaskCompletionRelease);
      }

      order_data->FinishProcessingOrderNumber(order_num);
    }
  }

  total_blocked_time_ += blocked_time;

  // Reset pointers after reacquiring the lock.
  sequence = GetSequence(sequence_id);
  if (sequence) {
    sequence->FinishTask();
  }
}

}  // namespace gpu
