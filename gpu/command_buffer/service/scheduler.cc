// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/scheduler.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/service/scheduler_task_runner.h"
#include "gpu/config/gpu_preferences.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"

namespace gpu {
namespace {
uint64_t GetTaskFlowId(uint32_t sequence_id, uint64_t order_num) {
  // Xor with a mask to reduce likelihood of flow id collision with
  // non-scheduler tasks. First 64-bits of SHA256 hash of "gpu::Scheduler",
  // interpreted as a big-endian integer. Python snippet:
  // hashlib.sha256(b'gpu::Scheduler').hexdigest()[:16]
  static constexpr uint64_t kMask = 0x03af62470b040902;
  return kMask ^ (static_cast<uint64_t>(sequence_id) << 32) ^ order_num;
}

perfetto::NamedTrack GetSchedulerTraceTrack(const SchedulerImpl* ptr) {
  return perfetto::NamedTrack::FromPointer("gpu::Scheduler", ptr);
}

void RecordSchedulerTimeHistogram(std::string_view histogram_name,
                                  std::string_view priority_str,
                                  base::TimeDelta sample) {
  constexpr base::TimeDelta kMin = base::Microseconds(10);
  constexpr base::TimeDelta kMax = base::Seconds(30);
  constexpr int kBuckets = 100;
  base::UmaHistogramCustomMicrosecondsTimes(histogram_name, sample, kMin, kMax,
                                            kBuckets);
  base::UmaHistogramCustomMicrosecondsTimes(
      base::StrCat({histogram_name, ".", priority_str, "Priority"}), sample,
      kMin, kMax, kBuckets);
}

void RecordSchedulerPercentageHistogram(std::string_view histogram_name,
                                        std::string_view priority_str,
                                        int sample) {
  base::UmaHistogramPercentage(histogram_name, sample);
  base::UmaHistogramPercentage(
      base::StrCat({histogram_name, ".", priority_str, "Priority"}), sample);
}

// Runs `task_closure` under `order_data` for `order_num`. If the task finishes
// (does not pause via ContinueTask), releases `release` (if any), finishes
// processing `order_num`, and returns true. Must be called without holding the
// scheduler lock because Begin/FinishProcessingOrderNumber can reenter the
// scheduler in Enable/DisableSequence.
bool RunTaskWithOrderNumber(SyncPointManager* sync_point_manager,
                            scoped_refptr<SyncPointOrderData> order_data,
                            uint64_t order_num,
                            const SyncToken& release,
                            base::OnceClosure task_closure) {
  order_data->BeginProcessingOrderNumber(order_num);
  std::move(task_closure).Run();
  if (!order_data->IsProcessingOrderNumber()) {
    return false;
  }
  if (release.HasData()) {
    sync_point_manager->EnsureFenceSyncReleased(
        release, ReleaseCause::kTaskCompletionRelease);
  }
  order_data->FinishProcessingOrderNumber(order_num);
  return true;
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
    : scheduler_(scheduler), sequence_id_(sequence_id) {
  scheduler_->SetSequencePriority(sequence_id, priority);
}
Scheduler::ScopedSetSequencePriority::~ScopedSetSequencePriority() {
  scheduler_->SetSequencePriority(
      sequence_id_, scheduler_->GetSequenceDefaultPriority(sequence_id_));
}

Scheduler::Sequence::Sequence(
    TaskGraph* task_graph,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    SchedulingPriority priority,
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id)
    : TaskGraph::Sequence(task_graph,
                          task_runner,
                          namespace_id,
                          command_buffer_id),
      task_runner_(std::move(task_runner)),
      default_priority_(priority),
      current_priority_(priority) {}

Scheduler::Sequence::~Sequence() = default;

bool Scheduler::Sequence::SetEnabled(bool enabled) {
  if (enabled_ == enabled) {
    return false;
  }
  enabled_ = enabled;
  auto track =
      perfetto::NamedTrack::FromPointer("gpu::Scheduler::Sequence", this);
  if (enabled) {
    TRACE_EVENT_BEGIN("gpu", "SequenceEnabled", track, "sequence_id",
                      sequence_id_.GetUnsafeValue());
  } else {
    TRACE_EVENT_END("gpu", track, "sequence_id", sequence_id_.GetUnsafeValue());
  }
  return true;
}

// static
std::unique_ptr<Scheduler> Scheduler::Create(
    SyncPointManager* sync_point_manager) {
  return std::make_unique<SchedulerImpl>(sync_point_manager);
}

Scheduler::Scheduler(SyncPointManager* sync_point_manager)
    : task_graph_(sync_point_manager) {}

Scheduler::~Scheduler() {
  base::AutoLock auto_lock(lock());
  DCHECK(scheduler_sequence_map_.empty());
}

SequenceId Scheduler::CreateSequence(
    SchedulingPriority priority,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return CreateSequence(priority, std::move(task_runner),
                        CommandBufferNamespace::INVALID,
                        /*command_buffer_id=*/{});
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
  if (sequence->SetEnabled(true)) {
    OnSequenceEnabledChanged(sequence);
  }
}

void Scheduler::DisableSequence(SequenceId sequence_id) {
  base::AutoLock auto_lock(lock());
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  if (sequence->SetEnabled(false)) {
    OnSequenceEnabledChanged(sequence);
  }
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

SchedulingPriority Scheduler::GetSequencePriority(SequenceId sequence_id) {
  base::AutoLock auto_lock(lock());
  Sequence* sequence = GetSequence(sequence_id);
  if (sequence) {
    return sequence->current_priority_;
  }
  return SchedulingPriority::kNormal;
}

void Scheduler::SetSequencePriority(SequenceId sequence_id,
                                    SchedulingPriority priority) {
  base::AutoLock auto_lock(lock());
  Sequence* sequence = GetSequence(sequence_id);
  if (sequence && sequence->current_priority_ != priority) {
    sequence->current_priority_ = priority;
    OnSequencePriorityChanged();
  }
}

void Scheduler::ScheduleTask(Task task) {
  base::AutoLock auto_lock(lock());
  ScheduleTaskLocked(std::move(task));
}

void Scheduler::ScheduleTask(SequenceId sequence_id,
                             base::OnceClosure task_closure) {
  ScheduleTask(Task(sequence_id, std::move(task_closure)));
}

void Scheduler::ScheduleTasks(std::vector<Task> tasks) {
  base::AutoLock auto_lock(lock());
  for (auto& task : tasks) {
    ScheduleTaskLocked(std::move(task));
  }
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

SchedulerImpl::SchedulingState::SchedulingState() = default;
SchedulerImpl::SchedulingState::SchedulingState(const SchedulingState& other) =
    default;
SchedulerImpl::SchedulingState::~SchedulingState() = default;

void SchedulerImpl::SchedulingState::WriteIntoTrace(
    perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("sequence_id", sequence_id.GetUnsafeValue());
  dict.Add("priority", SchedulingPriorityToString(priority));
  dict.Add("order_num", order_num);
}

bool SchedulerImpl::SchedulingState::operator==(
    const SchedulerImpl::SchedulingState& rhs) const {
  return std::tie(sequence_id, priority, order_num) ==
         std::tie(rhs.sequence_id, rhs.priority, rhs.order_num);
}

SchedulerImpl::PerThreadState::PerThreadState() = default;
SchedulerImpl::PerThreadState::PerThreadState(PerThreadState&& other) = default;
SchedulerImpl::PerThreadState::~PerThreadState() = default;
SchedulerImpl::PerThreadState& SchedulerImpl::PerThreadState::operator=(
    PerThreadState&& other) = default;

SchedulerImpl::Sequence::Sequence(
    SchedulerImpl* scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    SchedulingPriority priority,
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id)
    : Scheduler::Sequence(&scheduler->task_graph_,
                          std::move(task_runner),
                          priority,
                          namespace_id,
                          command_buffer_id),
      scheduler_(scheduler) {}

SchedulerImpl::Sequence::~Sequence() {
  for (const auto& wait_fence : wait_fences_) {
    Sequence* release_sequence =
        scheduler_->GetSequence(wait_fence.release_sequence_id);
    if (release_sequence) {
      scheduler_->TryScheduleSequence(release_sequence);
    }
  }
}

bool SchedulerImpl::Sequence::ShouldYieldTo(const Sequence* other) const {
  if (task_runner() != other->task_runner()) {
    return false;
  }
  if (!running()) {
    return false;
  }
  return SchedulingState::RunsBefore(other->scheduling_state_,
                                     scheduling_state_);
}

SchedulerImpl::SchedulingState SchedulerImpl::Sequence::SetScheduled() {
  DCHECK(HasTasksAndEnabled());
  DCHECK_NE(running_state_, RUNNING);

  running_state_ = SCHEDULED;

  scheduling_state_.sequence_id = sequence_id_;
  scheduling_state_.priority = current_priority();
  scheduling_state_.order_num = tasks_.front().order_num;

  return scheduling_state_;
}

void SchedulerImpl::Sequence::UpdateRunningPriority() {
  DCHECK_EQ(running_state_, RUNNING);
  scheduling_state_.priority = current_priority();
}

void SchedulerImpl::Sequence::ContinueTask(base::OnceClosure task_closure) {
  DCHECK_EQ(running_state_, RUNNING);
  TRACE_EVENT("gpu,toplevel.flow", "Scheduler::ContinueTask",
              perfetto::Flow::Global(GetTaskFlowId(
                  sequence_id_.value(), order_data_->current_order_num())));
  TaskGraph::Sequence::ContinueTask(std::move(task_closure));
}

uint64_t SchedulerImpl::Sequence::BeginTask(base::OnceClosure* task_closure) {
  DCHECK_EQ(running_state_, SCHEDULED);
  running_state_ = RUNNING;
  return TaskGraph::Sequence::BeginTask(task_closure);
}

void SchedulerImpl::Sequence::FinishTask() {
  DCHECK_EQ(running_state_, RUNNING);
  running_state_ = SCHEDULED;
  TaskGraph::Sequence::FinishTask();
}

void SchedulerImpl::Sequence::OnFrontTaskUnblocked(uint64_t order_num) {
  TRACE_EVENT(
      "gpu,toplevel.flow", "Scheduler::SequenceUnblocked",
      perfetto::Flow::Global(GetTaskFlowId(sequence_id_.value(), order_num)));
  scheduler_->TryScheduleSequence(this);
}

SchedulerImpl::SchedulerImpl(SyncPointManager* sync_point_manager)
    : Scheduler(sync_point_manager) {}

SchedulerImpl::~SchedulerImpl() {
  base::AutoLock auto_lock(lock());

  // Tasks posted to threads hold a raw "this" pointer to the Scheduler. Verify
  // none of the threads are still running by the time the scheduler is
  // destroyed.
  for (const auto& per_thread_state : per_thread_state_map_) {
    DCHECK(!per_thread_state.second.running);
  }
}

SequenceId SchedulerImpl::CreateSequence(
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

    per_thread_state_map_[sequence_ptr->task_runner()].num_sequences++;
  }
  return id;
}

void SchedulerImpl::DestroySequence(SequenceId sequence_id) {
  {
    base::AutoLock auto_lock(lock());
    auto sequence_it = scheduler_sequence_map_.find(sequence_id);
    CHECK(sequence_it != scheduler_sequence_map_.end());
    auto* sequence = static_cast<Sequence*>(sequence_it->second);
    if (scoped_refptr<SchedulerTaskRunner> scheduler_task_runner =
            sequence->scheduler_task_runner_) {
      // Reject new tasks before the sequence goes away. This must not be called
      // under the scheduler lock, because SchedulerTaskRunner calls
      // ScheduleTask() while holding its own lock. Keep
      // `sequence->scheduler_task_runner_` non-null while unlocked so a
      // concurrent GetTaskRunner() cannot allocate a new un-shut-down runner.
      sequence = nullptr;
      {
        base::AutoUnlock auto_unlock(lock());
        scheduler_task_runner->ShutDown();
      }
      sequence_it = scheduler_sequence_map_.find(sequence_id);
      CHECK(sequence_it != scheduler_sequence_map_.end());
      sequence = static_cast<Sequence*>(sequence_it->second);
    }

    auto per_thread_it = per_thread_state_map_.find(sequence->task_runner());
    CHECK(per_thread_it != per_thread_state_map_.end());
    auto& thread_state = per_thread_it->second;
    thread_state.num_sequences--;
    if (thread_state.num_sequences == 0) {
      per_thread_state_map_.erase(per_thread_it);
    }

    scheduler_sequence_map_.erase(sequence_it);
  }

  task_graph_.DestroySequence(sequence_id);
}

SchedulerImpl::Sequence* SchedulerImpl::GetSequence(SequenceId sequence_id) {
  return static_cast<Sequence*>(Scheduler::GetSequence(sequence_id));
}

SchedulerImpl::PerThreadState& SchedulerImpl::GetThreadState(
    base::SingleThreadTaskRunner* task_runner) {
  auto it = per_thread_state_map_.find(task_runner);
  CHECK(it != per_thread_state_map_.end());
  return it->second;
}

scoped_refptr<base::SequencedTaskRunner> SchedulerImpl::GetTaskRunner(
    SequenceId sequence_id) {
  base::AutoLock auto_lock(lock());
  Sequence* sequence = GetSequence(sequence_id);
  CHECK(sequence);
  if (!sequence->scheduler_task_runner_) {
    sequence->scheduler_task_runner_ =
        base::MakeRefCounted<SchedulerTaskRunner>(*this, sequence_id);
  }
  return sequence->scheduler_task_runner_;
}

void SchedulerImpl::ScheduleTaskLocked(Task task) {
  SequenceId sequence_id = task.sequence_id;
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);

  uint64_t order_num;
  if (task.task_callback) {
    order_num = sequence->AddTask(
        std::move(task.task_callback), std::move(task.sync_token_fences),
        task.release, std::move(task.report_callback));
  } else {
    order_num = sequence->AddTask(
        std::move(task.task_closure), std::move(task.sync_token_fences),
        task.release, std::move(task.report_callback));
  }

  TRACE_EVENT("gpu,toplevel.flow", "Scheduler::ScheduleTask",
              perfetto::Flow::Global(
                  GetTaskFlowId(sequence->sequence_id().value(), order_num)));

  TryScheduleSequence(sequence);
}

void SchedulerImpl::OnSequenceEnabledChanged(Scheduler::Sequence* sequence) {
  TryScheduleSequence(static_cast<Sequence*>(sequence));
}

bool SchedulerImpl::ShouldYield(SequenceId sequence_id) {
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

void SchedulerImpl::TryScheduleSequence(Sequence* sequence) {
  auto* task_runner = sequence->task_runner();
  auto& thread_state = GetThreadState(task_runner);

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
      TRACE_EVENT_BEGIN("gpu", "Scheduler::Running",
                        GetSchedulerTraceTrack(this));
      DVLOG(10) << "Waking up thread because there is work to do.";
      thread_state.running = true;
      thread_state.run_next_task_scheduled = base::TimeTicks::Now();
      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&SchedulerImpl::RunNextTask, base::Unretained(this)));
    }
  }
}

const std::vector<SchedulerImpl::SchedulingState>&
SchedulerImpl::GetSortedRunnableSequences(PerThreadState& thread_state) {
  std::vector<SchedulingState>& sorted_sequences =
      thread_state.sorted_sequences;

  sorted_sequences.clear();
  for (const auto& [_, base_sequence] : scheduler_sequence_map_) {
    auto* sequence = static_cast<Sequence*>(base_sequence);
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

bool SchedulerImpl::HasAnyUnblockedTasksOnRunner(
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

SchedulerImpl::Sequence* SchedulerImpl::FindNextTaskFromRoot(
    Sequence* root_sequence) {
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
  const uint64_t first_task_order_num = root_sequence->tasks_.front().order_num;
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

SchedulerImpl::Sequence* SchedulerImpl::FindNextTask() {
  auto* task_runner = base::SingleThreadTaskRunner::GetCurrentDefault().get();
  auto& thread_state = GetThreadState(task_runner);
  auto& sorted_sequences = GetSortedRunnableSequences(thread_state);

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
void SchedulerImpl::RunNextTask() {
  SequenceId sequence_id;
  DCHECK(sequence_id.is_null());

  {
    base::AutoLock auto_lock(lock());
    auto* task_runner = base::SingleThreadTaskRunner::GetCurrentDefault().get();

    DVLOG(10) << "RunNextTask: Task runner is " << (uint64_t)task_runner;

    auto it = per_thread_state_map_.find(task_runner);
    if (it == per_thread_state_map_.end()) {
      TRACE_EVENT_END("gpu", /*"Scheduler::Running"*/
                      GetSchedulerTraceTrack(this));

      DVLOG(10) << "Thread has no sequences. Sleeping.";
      return;
    }

    // Walk the job graph starting from the highest priority roots to find a
    // task to run.
    Sequence* sequence = FindNextTask();

    if (sequence == nullptr) {
      // If there is no sequence to run, it should mean that there are no
      // runnable sequences.
      // TODO(elgarawany): We shouldn't have run RunNextTask if there were no
      // runnable sequences. Change logic to check for that too (that changes
      // old behavior - so leaving for now).

      auto& thread_state = it->second;

      // TODO(crbug.com/40278526): this assert is firing frequently on
      // Release builds with dcheck_always_on on Intel Macs. It looks
      // like it happens when the browser drops frames.
      /*
      DCHECK(GetSortedRunnableSequences(thread_state).empty())
          << "RunNextTask should not have been called "
             "if it did not have any unblocked tasks.";
      */

      TRACE_EVENT_END("gpu", /*"Scheduler::Running"*/
                      GetSchedulerTraceTrack(this));

      DVLOG(10) << "Empty scheduling queue. Sleeping.";
      thread_state.running = false;
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

    auto it = per_thread_state_map_.find(task_runner);
    if (it == per_thread_state_map_.end()) {
      DVLOG(10) << "Thread has no sequences. Sleeping.";
      return;
    }
    auto& thread_state = it->second;

    if (!HasAnyUnblockedTasksOnRunner(task_runner)) {
      TRACE_EVENT_END("gpu", /*"Scheduler::Running"*/
                      GetSchedulerTraceTrack(this));
      DVLOG(10) << "Thread has no runnable sequences. Sleeping.";
      thread_state.running = false;
      return;
    }
    thread_state.run_next_task_scheduled = base::TimeTicks::Now();
    task_runner->PostTask(FROM_HERE, base::BindOnce(&SchedulerImpl::RunNextTask,
                                                    base::Unretained(this)));
  }
}

void SchedulerImpl::ExecuteSequence(const SequenceId sequence_id) {
  base::AutoLock auto_lock(lock());
  auto* task_runner = base::SingleThreadTaskRunner::GetCurrentDefault().get();
  auto& thread_state = GetThreadState(task_runner);

  DVLOG(10) << "Executing sequence " << sequence_id.value() << ".";
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  DCHECK(sequence->HasTasksAndEnabled());
  DCHECK_EQ(sequence->task_runner(), task_runner);

  // Subsampling these metrics reduced CPU utilization (crbug.com/1295441).
  const bool log_histograms = base::ShouldRecordSubsampledMetric(0.001);
  const std::string_view priority_str =
      SchedulingPriorityToString(sequence->default_priority());

  if (log_histograms) {
    base::TimeTicks now = base::TimeTicks::Now();

    RecordSchedulerTimeHistogram("GPU.Scheduler.ThreadSuspendedTime",
                                 priority_str,
                                 now - thread_state.run_next_task_scheduled);
    RecordSchedulerTimeHistogram("GPU.Scheduler.TaskDependencyTime",
                                 priority_str,
                                 sequence->FrontTaskWaitingDependencyDelta());
    // The delay between when the front task was ready to run (no more
    // dependencies) and now.
    RecordSchedulerTimeHistogram("GPU.Scheduler.TaskSchedulingDelayTime",
                                 priority_str,
                                 now - sequence->tasks_.front().running_ready);
    // The delay between when the front task was registered and now.
    RecordSchedulerTimeHistogram("GPU.Scheduler.TaskTotalDelayTime",
                                 priority_str,
                                 now - sequence->tasks_.front().registration);
  }

  base::OnceClosure task_closure;
  const uint64_t order_num = sequence->BeginTask(&task_closure);
  const SyncToken release = sequence->current_task_release();
  const uint64_t task_flow_id = GetTaskFlowId(sequence_id.value(), order_num);

  // Begin/FinishProcessingOrderNumber must be called with the lock released
  // because they can renter the scheduler in Enable/DisableSequence.
  scoped_refptr<SyncPointOrderData> order_data = sequence->order_data();

  // Unset pointers before releasing the lock to prevent accidental data race.
  sequence = nullptr;

  base::TimeDelta blocked_time;
  {
    base::AutoUnlock auto_unlock(lock());

    TRACE_EVENT("gpu,toplevel.flow", "Scheduler::RunTask",
                perfetto::Flow::Global(task_flow_id));

    if (RunTaskWithOrderNumber(task_graph_.sync_point_manager(),
                               std::move(order_data), order_num, release,
                               std::move(task_closure))) {
      TRACE_EVENT("gpu,toplevel.flow", "Scheduler::FinishTask",
                  perfetto::TerminatingFlow::Global(task_flow_id));
    }
  }
  total_blocked_time_ += blocked_time;

  // Reset pointers after reacquiring the lock.
  sequence = GetSequence(sequence_id);
  if (sequence) {
    sequence->FinishTask();
    if (log_histograms) {
      RecordSchedulerPercentageHistogram(
          "GPU.Scheduler.PercentConcurrentTaskExecution", priority_str,
          std::round(sequence->last_task_concurrency_ratio_ * 100));
    }
  }
}

}  // namespace gpu
