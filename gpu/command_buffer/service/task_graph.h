// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TASK_GRAPH_H_
#define GPU_COMMAND_BUFFER_SERVICE_TASK_GRAPH_H_

#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/gpu_export.h"

namespace gpu {

// TaskGraph keeps track of task sequences and the sync point dependencies
// between tasks.
class GPU_EXPORT TaskGraph {
 public:
  // A callback to be used for reporting when the task is ready to run (when the
  // dependencies have been solved).
  using ReportingCallback =
      base::OnceCallback<void(base::TimeTicks task_ready)>;

  class Sequence;

  explicit TaskGraph(SyncPointManager* sync_point_manager);

  TaskGraph(const TaskGraph&) = delete;
  TaskGraph& operator=(const TaskGraph&) = delete;

  ~TaskGraph();

  SequenceId CreateSequence(
      base::RepeatingClosure front_task_unblocked_callback)
      LOCKS_EXCLUDED(lock_);

  void AddSequence(std::unique_ptr<Sequence> sequence) LOCKS_EXCLUDED(lock_);

  void DestroySequence(SequenceId sequence_id) LOCKS_EXCLUDED(lock_);

  SyncPointManager* sync_point_manager() { return sync_point_manager_; }

  // Returns the lock that must be held when accessing Sequence instances
  // directly. Please also see comments of GetSequence().
  base::Lock& lock() const LOCK_RETURNED(lock_) { return lock_; }

  // Usage of the returned sequence must be guarded by `lock_`. Please see
  // locking annotation of individual methods.
  //
  // If the caller would like to hold onto the returned sequence pointer, it is
  // responsible to ensure that during the period it holds onto the pointer, no
  // one calls DestroySequence() to destroy this sequence.
  Sequence* GetSequence(SequenceId sequence_id) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  class GPU_EXPORT Sequence {
   public:
    // Notes regarding `front_task_unblocked_callback`:
    // - It could be called from any thread.
    // - To avoid reentrancy, it is not called by AddTask() or FinishTask(),
    //   even if those methods result a new front task which is not blocked.
    // - It is called while holding `TaskGraph::lock_`.
    Sequence(TaskGraph* task_graph,
             base::RepeatingClosure front_task_unblocked_callback);

    Sequence(const Sequence&) = delete;
    Sequence& operator=(const Sequence&) = delete;

    virtual ~Sequence() EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    SequenceId sequence_id() const { return sequence_id_; }

    const scoped_refptr<SyncPointOrderData>& order_data() const {
      return order_data_;
    }

    bool HasTasks() const EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_) {
      return !tasks_.empty();
    }

    // Enqueues a task in the sequence and returns the generated order number.
    virtual uint32_t AddTask(base::OnceClosure closure,
                             std::vector<SyncToken> wait_fences,
                             ReportingCallback report_callback)
        EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    virtual uint32_t BeginTask(base::OnceClosure* closure)
        EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    // Should be called after running the closure returned by BeginTask().
    virtual void FinishTask() EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    // Continues running the current task with the given closure. Must be called
    // in between BeginTask() and FinishTask().
    virtual void ContinueTask(base::OnceClosure closure)
        EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    // Sets the first dependency added time on the last task if it wasn't
    // already set, no-op otherwise.
    void SetLastTaskFirstDependencyTimeIfNeeded()
        EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    // The time delta it took for the front task's dependencies to be completed.
    base::TimeDelta FrontTaskWaitingDependencyDelta()
        EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    // The delay between when the front task was ready to run (no more
    // dependencies) and now. This is used when the task is actually started to
    // check for low scheduling delays.
    base::TimeDelta FrontTaskSchedulingDelay()
        EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    // Removes a waiting sync token fence.
    void RemoveWaitFence(const SyncToken& sync_token,
                         uint32_t order_num,
                         SequenceId release_sequence_id)
        EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    // Returns true if the sequence is not empty, and the first task does not
    // have any pending dependencies.
    bool IsFrontTaskUnblocked() const
        EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    // This is the same lock for the entire `task_graph_`.
    base::Lock& lock() const LOCK_RETURNED(&TaskGraph::lock_) {
      return task_graph_->lock();
    }

   protected:
    friend class TaskGraph;

    struct WaitFence {
      // A wait on `sync_token` that blocks tasks with order number <=
      // `order_num`. The `sync_token` is released on the sequence identified
      // by `release_sequence_id`.
      WaitFence(const SyncToken& sync_token,
                uint32_t order_num,
                SequenceId release_sequence_id);
      WaitFence(WaitFence&& other);

      ~WaitFence();

      WaitFence& operator=(WaitFence&& other);

      SyncToken sync_token;
      uint32_t order_num;
      SequenceId release_sequence_id;

      bool operator==(const WaitFence& other) const {
        return std::tie(order_num, release_sequence_id, sync_token) ==
               std::tie(other.order_num, release_sequence_id, other.sync_token);
      }

      bool operator<(const WaitFence& other) const {
        return std::tie(order_num, release_sequence_id, sync_token) <
               std::tie(other.order_num, release_sequence_id, other.sync_token);
      }
    };

    using WaitFenceSet = base::flat_set<WaitFence>;
    using WaitFenceConstIter = typename WaitFenceSet::const_iterator;

    struct Task {
      Task(Task&& other);
      Task(base::OnceClosure closure,
           uint32_t order_num,
           ReportingCallback report_callback);
      ~Task();
      Task& operator=(Task&& other);

      base::OnceClosure closure;
      uint32_t order_num;

      ReportingCallback report_callback;
      // Note: this time is only correct once the last fence has been removed,
      // as it is updated for all fences.
      base::TimeTicks running_ready = base::TimeTicks::Now();
      base::TimeTicks first_dependency_added;
      base::TimeTicks registration = base::TimeTicks::Now();
    };

    RAW_PTR_EXCLUSION TaskGraph* const task_graph_ = nullptr;
    const scoped_refptr<SyncPointOrderData> order_data_;
    const SequenceId sequence_id_;

    // Called while holding `Taskgraph::lock_`.
    const base::RepeatingClosure front_task_unblocked_callback_;

    // Deque of tasks. Tasks are inserted at the back with increasing order
    // number generated from SyncPointOrderData. If a running task needs to be
    // continued, it is inserted at the front with the same order number.
    base::circular_deque<Task> tasks_ GUARDED_BY(&TaskGraph::lock_);

    // Map of fences that this sequence is waiting on. Fences are ordered in
    // increasing order number but may be removed out of order. Tasks are
    // blocked if there's a wait fence with order number less than or equal to
    // the task's order number.
    WaitFenceSet wait_fences_ GUARDED_BY(&TaskGraph::lock_);
  };

 private:
  void SyncTokenFenceReleased(const SyncToken& sync_token,
                              uint32_t order_num,
                              SequenceId release_sequence_id,
                              SequenceId waiting_sequence_id)
      LOCKS_EXCLUDED(lock_);

  mutable base::Lock lock_;

  const raw_ptr<SyncPointManager> sync_point_manager_;

  base::flat_map<SequenceId, std::unique_ptr<Sequence>> sequence_map_
      GUARDED_BY(lock_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TASK_GRAPH_H_
