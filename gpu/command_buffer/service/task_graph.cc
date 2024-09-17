// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/task_graph.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"

namespace gpu {
namespace {

void UpdateReleaseCount(
    base::flat_map<SyncPointClientId, uint64_t>* release_map,
    const SyncPointClientId& client_id,
    uint64_t release) {
  auto iter = release_map->find(client_id);
  if (iter == release_map->end()) {
    release_map->insert({client_id, release});
  } else if (iter->second < release) {
    iter->second = release;
  }
}

}  // namespace

FenceSyncReleaseDelegate::FenceSyncReleaseDelegate(
    SyncPointManager* sync_point_manager)
    : sync_point_manager_(sync_point_manager) {}

void FenceSyncReleaseDelegate::Release() {
  sync_point_manager_->EnsureFenceSyncReleased(
      release_upperbound_, ReleaseCause::kExplicitClientRelease);
}

void FenceSyncReleaseDelegate::Release(uint64_t release) {
  if (release > release_upperbound_.release_count()) {
    LOG(DFATAL) << "Attempt to release fence sync with a release count larger "
                   "than what was specified at task registration. Requested: "
                << release
                << "; registered: " << release_upperbound_.release_count();
    release = release_upperbound_.release_count();
  }

  SyncToken request(release_upperbound_.namespace_id(),
                    release_upperbound_.command_buffer_id(), release);
  sync_point_manager_->EnsureFenceSyncReleased(
      request, ReleaseCause::kExplicitClientRelease);
}

void FenceSyncReleaseDelegate::Reset(const SyncToken& release_upperbound) {
  release_upperbound_ = release_upperbound;
}

ScopedSyncPointClientState::ScopedSyncPointClientState(
    TaskGraph* task_graph,
    SequenceId sequence_id,
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id)
    : task_graph_(task_graph),
      sequence_id_(sequence_id),
      namespace_id_(namespace_id),
      command_buffer_id_(command_buffer_id) {}

ScopedSyncPointClientState::~ScopedSyncPointClientState() {
  Reset();
}

ScopedSyncPointClientState::ScopedSyncPointClientState(
    ScopedSyncPointClientState&& other)
    : task_graph_(other.task_graph_),
      sequence_id_(other.sequence_id_),
      namespace_id_(other.namespace_id_),
      command_buffer_id_(other.command_buffer_id_) {
  other.task_graph_ = nullptr;
}

ScopedSyncPointClientState& ScopedSyncPointClientState::operator=(
    ScopedSyncPointClientState&& other) {
  if (&other != this) {
    task_graph_ = other.task_graph_;
    other.task_graph_ = nullptr;
    sequence_id_ = other.sequence_id_;
    namespace_id_ = other.namespace_id_;
    command_buffer_id_ = other.command_buffer_id_;
  }
  return *this;
}

void ScopedSyncPointClientState::Reset() {
  if (!task_graph_) {
    return;
  }

  task_graph_->DestroySyncPointClientState(sequence_id_, namespace_id_,
                                           command_buffer_id_);
  task_graph_ = nullptr;
}

TaskGraph::Sequence::Task::Task(base::OnceClosure task_closure,
                                uint32_t order_num,
                                const SyncToken& release,
                                ReportingCallback report_callback)
    : task_closure(std::move(task_closure)),
      order_num(order_num),
      release(release),
      report_callback(std::move(report_callback)) {}

TaskGraph::Sequence::Task::Task(Task&& other) = default;
TaskGraph::Sequence::Task::~Task() {
  CHECK(report_callback.is_null());
}

TaskGraph::Sequence::Task& TaskGraph::Sequence::Task::operator=(Task&& other) =
    default;

TaskGraph::Sequence::WaitFence::WaitFence(const SyncToken& sync_token,
                                          uint32_t order_num,
                                          SequenceId release_sequence_id)
    : sync_token(sync_token),
      order_num(order_num),
      release_sequence_id(release_sequence_id) {}
TaskGraph::Sequence::WaitFence::WaitFence(WaitFence&& other) = default;
TaskGraph::Sequence::WaitFence::~WaitFence() = default;
TaskGraph::Sequence::WaitFence& TaskGraph::Sequence::WaitFence::operator=(
    WaitFence&& other) = default;

TaskGraph::Sequence::Sequence(
    TaskGraph* task_graph,
    base::RepeatingClosure front_task_unblocked_callback,
    scoped_refptr<base::SingleThreadTaskRunner> validation_runner,
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id)
    : task_graph_(task_graph),
      order_data_(task_graph_->sync_point_manager_->CreateSyncPointOrderData()),
      sequence_id_(order_data_->sequence_id()),
      front_task_unblocked_callback_(std::move(front_task_unblocked_callback)),
      release_delegate_(task_graph->sync_point_manager()) {
  if (task_graph_->graph_validation_enabled()) {
    validation_timer_ = base::MakeRefCounted<RetainingOneShotTimerHolder>(
        kMaxValidationDelay, kMinValidationDelay, std::move(validation_runner),
        base::BindRepeating(&TaskGraph::ValidateSequenceTaskFenceDeps,
                            base::Unretained(task_graph),
                            base::Unretained(this)));
  }

  if (namespace_id != CommandBufferNamespace::INVALID) {
    sync_point_states_.push_back(
        task_graph_->sync_point_manager()->CreateSyncPointClientState(
            namespace_id, command_buffer_id, sequence_id_));
  }
}

TaskGraph::Sequence::~Sequence() {
}

ScopedSyncPointClientState TaskGraph::Sequence::CreateSyncPointClientState(
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id) {
  sync_point_states_.push_back(
      task_graph_->sync_point_manager()->CreateSyncPointClientState(
          namespace_id, command_buffer_id, sequence_id_));
  return ScopedSyncPointClientState(task_graph_, sequence_id_, namespace_id,
                                    command_buffer_id);
}

uint32_t TaskGraph::Sequence::AddTask(TaskCallback task_callback,
                                      std::vector<SyncToken> wait_fences,
                                      const SyncToken& release,
                                      ReportingCallback report_callback) {
  return AddTask(CreateTaskClosure(std::move(task_callback)),
                 std::move(wait_fences), release, std::move(report_callback));
}

uint32_t TaskGraph::Sequence::AddTask(base::OnceClosure task_closure,
                                      std::vector<SyncToken> wait_fences,
                                      const SyncToken& release,
                                      ReportingCallback report_callback) {
  uint32_t order_num = order_data_->GenerateUnprocessedOrderNumber();
  tasks_.push_back({std::move(task_closure), order_num, release,
                    std::move(report_callback)});

  for (const SyncToken& sync_token : ReduceSyncTokens(wait_fences)) {
    SequenceId release_sequence_id =
        task_graph_->sync_point_manager_->GetSyncTokenReleaseSequenceId(
            sync_token);
    // base::Unretained is safe here since all sequences and corresponding sync
    // point callbacks will be released before the task graph is destroyed (even
    // though sync point manager itself outlives the task graph briefly).
    if (task_graph_->sync_point_manager_->Wait(
            sync_token, sequence_id_, order_num,
            base::BindOnce(&TaskGraph::SyncTokenFenceReleased,
                           base::Unretained(task_graph_), sync_token, order_num,
                           release_sequence_id, sequence_id_))) {
      auto it = wait_fences_.find(
          WaitFence{sync_token, order_num, release_sequence_id});
      if (it == wait_fences_.end()) {
        wait_fences_.emplace(sync_token, order_num, release_sequence_id);
      }
      SetLastTaskFirstDependencyTimeIfNeeded();
    }
  }

  if (tasks_.size() == 1) {
    // The queue just got the first element, update the timer if necessary.
    UpdateValidationTimer();
  }

  return order_num;
}

uint32_t TaskGraph::Sequence::BeginTask(base::OnceClosure* task_closure) {
  DCHECK(task_closure);
  DCHECK(!tasks_.empty());

  DVLOG(10) << "Sequence " << sequence_id() << " is now running.";
  *task_closure = std::move(tasks_.front().task_closure);
  uint32_t order_num = tasks_.front().order_num;
  current_task_release_ = tasks_.front().release;
  release_delegate_.Reset(current_task_release_);

  if (!tasks_.front().report_callback.is_null()) {
    std::move(tasks_.front().report_callback).Run(tasks_.front().running_ready);
  }
  tasks_.pop_front();

  return order_num;
}

void TaskGraph::Sequence::FinishTask() {
  DVLOG(10) << "Sequence " << sequence_id() << " is now ending.";
  SyncToken release = current_task_release_;
  current_task_release_.Clear();
  UpdateValidationTimer();
}

void TaskGraph::Sequence::ContinueTask(TaskCallback task_callback) {
  ContinueTask(CreateTaskClosure(std::move(task_callback)));
}

void TaskGraph::Sequence::ContinueTask(base::OnceClosure task_closure) {
  uint32_t order_num = order_data_->current_order_num();

  tasks_.push_front({std::move(task_closure), order_num, current_task_release_,
                     ReportingCallback()});
  current_task_release_.Clear();
  order_data_->PauseProcessingOrderNumber(order_num);
}

void TaskGraph::Sequence::SetLastTaskFirstDependencyTimeIfNeeded() {
  DCHECK(!tasks_.empty());
  if (tasks_.back().first_dependency_added.is_null()) {
    // Fence are always added for the last task (which should always exists).
    tasks_.back().first_dependency_added = base::TimeTicks::Now();
  }
}

base::TimeDelta TaskGraph::Sequence::FrontTaskWaitingDependencyDelta() {
  DCHECK(!tasks_.empty());
  if (tasks_.front().first_dependency_added.is_null()) {
    // didn't wait for dependencies.
    return base::TimeDelta();
  }
  return tasks_.front().running_ready - tasks_.front().first_dependency_added;
}

base::TimeDelta TaskGraph::Sequence::FrontTaskSchedulingDelay() {
  DCHECK(!tasks_.empty());
  return base::TimeTicks::Now() - tasks_.front().running_ready;
}

void TaskGraph::Sequence::RemoveWaitFence(const SyncToken& sync_token,
                                          uint32_t order_num,
                                          SequenceId release_sequence_id) {
  DVLOG(10) << "Sequence " << sequence_id_.value()
            << " removing wait fence that was released by sequence "
            << release_sequence_id.value() << ".";
  auto it =
      wait_fences_.find(WaitFence{sync_token, order_num, release_sequence_id});
  if (it == wait_fences_.end()) {
    return;
  }

  wait_fences_.erase(it);
  for (auto& task : tasks_) {
    if (order_num == task.order_num) {
      // The fence applies to this task, bump the readiness timestamp.
      task.running_ready = base::TimeTicks::Now();
      break;
    } else if (order_num < task.order_num) {
      // Updated all task related to this fence.
      break;
    }
  }

  DCHECK(!tasks_.empty());
  if (order_num == tasks_.front().order_num && IsFrontTaskUnblocked()) {
    front_task_unblocked_callback_.Run();
  }
}

bool TaskGraph::Sequence::IsFrontTaskUnblocked() const {
  return !tasks_.empty() &&
         (wait_fences_.empty() ||
          wait_fences_.begin()->order_num > tasks_.front().order_num);
}

void TaskGraph::Sequence::Destroy() {
  std::vector<scoped_refptr<SyncPointClientState>> sync_point_states;
  {
    base::AutoLock auto_lock(task_graph_->lock());
    sync_point_states_.swap(sync_point_states);
  }

  if (validation_timer_) {
    validation_timer_->DestroyTimer();
  }

  for (auto& state : sync_point_states) {
    state->Destroy();
  }

  order_data_->Destroy();
}

scoped_refptr<SyncPointClientState>
TaskGraph::Sequence::TakeSyncPointClientState(
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id) {
  scoped_refptr<SyncPointClientState> sync_point_state;
  for (auto iter = sync_point_states_.begin(); iter != sync_point_states_.end();
       ++iter) {
    if ((*iter)->namespace_id() == namespace_id &&
        (*iter)->command_buffer_id() == command_buffer_id) {
      sync_point_state = std::move(*iter);
      sync_point_states_.erase(iter);
      break;
    }
  }

  return sync_point_state;
}

void TaskGraph::Sequence::UpdateValidationTimer() {
  if (!task_graph_->graph_validation_enabled()) {
    return;
  }

  if (!HasTasks()) {
    return;
  }

  validation_timer_->ResetTimerIfNecessary();
}

std::pair<TaskGraph::Sequence::WaitFenceConstIter,
          TaskGraph::Sequence::WaitFenceConstIter>
TaskGraph::Sequence::GetTaskWaitFences(const Task& task) const {
  struct Comp {
    bool operator()(const WaitFence& left, uint32_t right) {
      return left.order_num < right;
    }
    bool operator()(uint32_t left, const WaitFence& right) {
      return left < right.order_num;
    }
  };
  return std::equal_range(wait_fences_.begin(), wait_fences_.end(),
                          task.order_num, Comp{});
}

const TaskGraph::Sequence::Task* TaskGraph::Sequence::FindReleaseTask(
    const SyncToken& sync_token) const {
  for (const auto& task : tasks_) {
    if (task.release.HasData() &&
        task.release.namespace_id() == sync_token.namespace_id() &&
        task.release.command_buffer_id() == sync_token.command_buffer_id() &&
        task.release.release_count() >= sync_token.release_count()) {
      return &task;
    }
  }
  return nullptr;
}

base::OnceClosure TaskGraph::Sequence::CreateTaskClosure(
    TaskCallback task_callback) {
  return base::BindOnce(std::move(task_callback),
                        base::Unretained(&release_delegate_));
}
TaskGraph::TaskGraph(SyncPointManager* sync_point_manager)
    : sync_point_manager_(sync_point_manager) {}

TaskGraph::~TaskGraph() {
  base::AutoLock auto_lock(lock_);
  DCHECK(sequence_map_.empty());
}

SequenceId TaskGraph::CreateSequence(
    base::RepeatingClosure front_task_unblocked_callback,
    scoped_refptr<base::SingleThreadTaskRunner> validation_runner) {
  return CreateSequence(
      std::move(front_task_unblocked_callback), std::move(validation_runner),
      CommandBufferNamespace::INVALID, /*command_buffer_id=*/{});
}

SequenceId TaskGraph::CreateSequence(
    base::RepeatingClosure front_task_unblocked_callback,
    scoped_refptr<base::SingleThreadTaskRunner> validation_runner,
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id) {
  base::AutoLock auto_lock(lock_);
  auto sequence = std::make_unique<Sequence>(
      this, std::move(front_task_unblocked_callback),
      std::move(validation_runner), namespace_id, command_buffer_id);
  SequenceId id = sequence->sequence_id();
  sequence_map_.emplace(id, std::move(sequence));
  return id;
}

void TaskGraph::AddSequence(std::unique_ptr<Sequence> sequence) {
  base::AutoLock auto_lock(lock_);
  SequenceId id = sequence->sequence_id();
  sequence_map_.emplace(id, std::move(sequence));
}

ScopedSyncPointClientState TaskGraph::CreateSyncPointClientState(
    SequenceId sequence_id,
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id) {
  base::AutoLock auto_lock(lock_);
  auto* sequence = GetSequence(sequence_id);
  CHECK(sequence);
  return sequence->CreateSyncPointClientState(namespace_id, command_buffer_id);
}

void TaskGraph::DestroySequence(SequenceId sequence_id) {
  base::AutoLock auto_lock(lock_);
  std::unique_ptr<Sequence> sequence;

  // We want to destroy the sequence after removing it from the sequence map
  // so that looping over the sequence map does not access a destroyed
  // sequence.
  sequence = std::move(sequence_map_.at(sequence_id));
  CHECK(sequence);
  sequence_map_.erase(sequence_id);

  {
    // Thread annotation macros don't recognize base::AutoUnlock.
    lock_.Release();
    sequence->Destroy();
    lock_.Acquire();
  }
}

TaskGraph::Sequence* TaskGraph::GetSequence(SequenceId sequence_id) {
  auto it = sequence_map_.find(sequence_id);
  if (it != sequence_map_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void TaskGraph::SyncTokenFenceReleased(const SyncToken& sync_token,
                                       uint32_t order_num,
                                       SequenceId release_sequence_id,
                                       SequenceId waiting_sequence_id) {
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(waiting_sequence_id);

  if (sequence) {
    sequence->RemoveWaitFence(sync_token, order_num, release_sequence_id);
  }
}

void TaskGraph::DestroySyncPointClientState(SequenceId sequence_id,
                                            CommandBufferNamespace namespace_id,
                                            CommandBufferId command_buffer_id) {
  scoped_refptr<SyncPointClientState> sync_point_client_state;
  {
    base::AutoLock auto_lock(lock_);
    Sequence* sequence = GetSequence(sequence_id);

    if (sequence) {
      sync_point_client_state =
          sequence->TakeSyncPointClientState(namespace_id, command_buffer_id);
    }
  }

  if (sync_point_client_state) {
    sync_point_client_state->Destroy();
  }
}

void TaskGraph::ValidateSequenceTaskFenceDeps(Sequence* root_sequence) {
  DCHECK(graph_validation_enabled());

  DVLOG(10) << "Validation: root sequence " << root_sequence->sequence_id();

  // Releases that need to be forcefully done to avoid invalid waits.
  ReleaseMap force_releases;
  {
    base::AutoLock auto_lock(lock_);

    if (!root_sequence->HasTasks()) {
      // Return without updating the timer. When the sequence becomes non-empty,
      // or after finishing the ongoing task, the validation timer will be
      // reset as needed.
      return;
    }

    // Releases that are supposed to happen once the validated tasks are
    // executed.
    ReleaseMap pending_releases;

    ValidateStateMap validate_states;

    base::TimeTicks now = base::TimeTicks::Now();
    for (Sequence::TaskIter task_iter = root_sequence->tasks_.begin();
         task_iter != root_sequence->tasks_.end(); ++task_iter) {
      if (now - task_iter->registration <= kMinValidationDelay) {
        break;
      }
      if (task_iter->validated) {
        continue;
      }

      ValidateTaskFenceDeps(root_sequence, task_iter, &pending_releases,
                            &force_releases, &validate_states);
    }
  }

  for (const auto& [client_id, release] : force_releases) {
    sync_point_manager_->EnsureFenceSyncReleased(
        {client_id.namespace_id, client_id.command_buffer_id, release},
        ReleaseCause::kForceRelease);
  }
}

void TaskGraph::ValidateTaskFenceDeps(
    Sequence* sequence,
    TaskGraph::Sequence::TaskIter task_iter,
    TaskGraph::ReleaseMap* pending_releases,
    TaskGraph::ReleaseMap* force_releases,
    TaskGraph::ValidateStateMap* validate_states) {
  Sequence::Task& task = *task_iter;

  auto& validate_state =
      GetSequenceValidateState(validate_states, pending_releases, sequence);
  CHECK(validate_state.next_to_validate == task_iter);
  CHECK(!validate_state.validating);

  validate_state.validating = true;

  DVLOG(10) << "Validation: sequence " << sequence->sequence_id() << "; task "
            << task.order_num;

  auto [fence_begin, fence_end] = sequence->GetTaskWaitFences(task);

  for (auto fence_iter = fence_begin; fence_iter != fence_end; ++fence_iter) {
    const Sequence::WaitFence& fence = *fence_iter;
    const SyncPointClientId client_id = fence.sync_token.GetClientId();

    Sequence* release_sequence = GetSequence(fence.release_sequence_id);

    auto& release_validate_state = GetSequenceValidateState(
        validate_states, pending_releases, release_sequence);

    // This should happen after the GetSequenceValidateState() call above, which
    // ensures that `pending_releases` has been updated for `release_sequence`.
    // Please see comments of GetSequenceValidateState().
    auto pending_release_iter = pending_releases->find(client_id);
    if (pending_release_iter != pending_releases->end() &&
        pending_release_iter->second >= fence.sync_token.release_count()) {
      continue;
    }

    const Sequence::Task* release_task =
        release_sequence ? release_sequence->FindReleaseTask(fence.sync_token)
                         : nullptr;

    if (!release_task) {
      // Null `release_task` indicates the wait-without-release case: A wait
      // fence is waited on for some time, but the release hasn't been
      // registered.
      // Forcefully release the fence.
      UpdateReleaseCount(pending_releases, client_id,
                         fence.sync_token.release_count());
      UpdateReleaseCount(force_releases, client_id,
                         fence.sync_token.release_count());

      LOG(ERROR) << "Validation: wait-without-release detected. Forcefully "
                    "release fence: Release sequence "
                 << release_sequence->sequence_id() << "; sync token "
                 << fence.sync_token.ToDebugString();
    } else {
      if (release_validate_state.validating) {
        // Circular dependency detected.
        // Forcefully release the fence to break the cycle.
        UpdateReleaseCount(pending_releases, client_id,
                           fence.sync_token.release_count());
        UpdateReleaseCount(force_releases, client_id,
                           fence.sync_token.release_count());

        LOG(ERROR) << "Validation: cycle detected. Forcefully release fence: "
                      "release sequence "
                   << release_sequence->sequence_id() << "; sync token "
                   << fence.sync_token.ToDebugString();
      } else {
        // In order for `release_task` to get a chance to run, all prior tasks
        // in the same sequence must be able to run, so validate them all.
        for (auto dep_task_iter = release_validate_state.next_to_validate;
             dep_task_iter != release_sequence->tasks_.end(); ++dep_task_iter) {
          if (dep_task_iter->order_num > release_task->order_num) {
            break;
          }
          ValidateTaskFenceDeps(release_sequence, dep_task_iter,
                                pending_releases, force_releases,
                                validate_states);
        }
      }
    }
  }

  if (task.release.HasData()) {
    UpdateReleaseCount(pending_releases, task.release.GetClientId(),
                       task.release.release_count());
  }
  task.validated = true;

  validate_state.validating = false;
  validate_state.next_to_validate = std::next(task_iter);
}

TaskGraph::ValidateState& TaskGraph::GetSequenceValidateState(
    TaskGraph::ValidateStateMap* validate_states,
    ReleaseMap* pending_releases,
    Sequence* sequence) {
  auto state_iter = validate_states->find(sequence->sequence_id());
  if (state_iter != validate_states->end()) {
    return state_iter->second;
  }

  auto& validate_state = (*validate_states)[sequence->sequence_id()];

  // If there is a currently ongoing task, add its release to
  // `pending_releases`.
  auto& current_task_release = sequence->current_task_release_;
  if (current_task_release.HasData()) {
    UpdateReleaseCount(pending_releases, current_task_release.GetClientId(),
                       current_task_release.release_count());
  }

  validate_state.next_to_validate = sequence->tasks_.end();
  for (auto task_iter = sequence->tasks_.begin();
       task_iter != sequence->tasks_.end(); ++task_iter) {
    if (!task_iter->validated) {
      validate_state.next_to_validate = task_iter;
      break;
    }

    // If the task has been validated before, its release is considered
    // satisfiable and therefore directly added to `pending_releases`.
    if (task_iter->release.HasData()) {
      UpdateReleaseCount(pending_releases, task_iter->release.GetClientId(),
                         task_iter->release.release_count());
    }
  }

  return validate_state;
}

}  // namespace gpu
