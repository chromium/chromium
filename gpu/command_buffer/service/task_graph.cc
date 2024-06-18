// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/task_graph.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"

namespace gpu {

TaskGraph::Sequence::Task::Task(base::OnceClosure closure,
                                uint32_t order_num,
                                ReportingCallback report_callback)
    : closure(std::move(closure)),
      order_num(order_num),
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
    base::RepeatingClosure front_task_unblocked_callback)
    : task_graph_(task_graph),
      order_data_(task_graph_->sync_point_manager_->CreateSyncPointOrderData()),
      sequence_id_(order_data_->sequence_id()),
      front_task_unblocked_callback_(std::move(front_task_unblocked_callback)) {
}

TaskGraph::Sequence::~Sequence() {
  order_data_->Destroy();
}

uint32_t TaskGraph::Sequence::AddTask(base::OnceClosure closure,
                                      std::vector<SyncToken> wait_fences,
                                      ReportingCallback report_callback) {
  uint32_t order_num = order_data_->GenerateUnprocessedOrderNumber();
  tasks_.push_back({std::move(closure), order_num, std::move(report_callback)});

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

  return order_num;
}

uint32_t TaskGraph::Sequence::BeginTask(base::OnceClosure* closure) {
  DCHECK(closure);
  DCHECK(!tasks_.empty());

  DVLOG(10) << "Sequence " << sequence_id() << " is now running.";
  *closure = std::move(tasks_.front().closure);
  uint32_t order_num = tasks_.front().order_num;
  if (!tasks_.front().report_callback.is_null()) {
    std::move(tasks_.front().report_callback).Run(tasks_.front().running_ready);
  }
  tasks_.pop_front();

  return order_num;
}

void TaskGraph::Sequence::FinishTask() {
  DVLOG(10) << "Sequence " << sequence_id() << " is now ending.";
}

void TaskGraph::Sequence::ContinueTask(base::OnceClosure closure) {
  uint32_t order_num = order_data_->current_order_num();

  tasks_.push_front({std::move(closure), order_num, ReportingCallback()});
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

TaskGraph::TaskGraph(SyncPointManager* sync_point_manager)
    : sync_point_manager_(sync_point_manager) {}

TaskGraph::~TaskGraph() {
  base::AutoLock auto_lock(lock_);
  DCHECK(sequence_map_.empty());
}

SequenceId TaskGraph::CreateSequence(
    base::RepeatingClosure front_task_unblocked_callback) {
  base::AutoLock auto_lock(lock_);
  auto sequence = std::make_unique<Sequence>(
      this, std::move(front_task_unblocked_callback));
  SequenceId id = sequence->sequence_id();
  sequence_map_.emplace(id, std::move(sequence));
  return id;
}

void TaskGraph::AddSequence(std::unique_ptr<Sequence> sequence) {
  base::AutoLock auto_lock(lock_);
  SequenceId id = sequence->sequence_id();
  sequence_map_.emplace(id, std::move(sequence));
}

void TaskGraph::DestroySequence(SequenceId sequence_id) {
  base::AutoLock auto_lock(lock_);

  // We want to destroy the sequence after removing it from the sequence map
  // so that looping over the sequence map does not access a destroyed
  // sequence.
  std::unique_ptr<Sequence> sequence = std::move(sequence_map_.at(sequence_id));
  CHECK(sequence);
  sequence_map_.erase(sequence_id);
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

}  // namespace gpu
