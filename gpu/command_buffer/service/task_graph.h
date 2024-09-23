// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TASK_GRAPH_H_
#define GPU_COMMAND_BUFFER_SERVICE_TASK_GRAPH_H_

#include <map>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/retaining_one_shot_timer_holder.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/gpu_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gpu {

class TaskGraph;

// FenceSyncReleaseDelegate can be used to release fence sync during task
// execution.
class GPU_EXPORT FenceSyncReleaseDelegate {
 public:
  explicit FenceSyncReleaseDelegate(SyncPointManager* sync_point_manager);

  // Releases fence sync with the release count specified at task registration.
  void Release();
  // Releases fence sync with release count `release`, which should be no
  // greater than the release count specified at task registration.
  void Release(uint64_t release);

  // Used by TaskGraph::Sequence.
  void Reset(const SyncToken& release_upperbound);

 private:
  const raw_ptr<SyncPointManager> sync_point_manager_;
  SyncToken release_upperbound_;
};

// The task can use `release_delegate` during its execution to release fence
// sync specified at task registration. Note:
// * Calling `release_delegate` is optional. If not called, release will happen
//   right after task completion.
// * `release_delegate` is not supposed to be stored or used after the task
//   callback returns.
using TaskCallback =
    base::OnceCallback<void(FenceSyncReleaseDelegate* release_delegate)>;

// ScopedSyncPointClientState (if valid) destroys the corresponding
// SyncPointClientState when it is destructed. It is move-only to avoid
// calling destroy multiple times.
class GPU_EXPORT ScopedSyncPointClientState {
 public:
  ScopedSyncPointClientState() = default;

  // `task_graph` must outlive this object.
  ScopedSyncPointClientState(TaskGraph* task_graph,
                             SequenceId sequence_id,
                             CommandBufferNamespace namespace_id,
                             CommandBufferId command_buffer_id);

  ~ScopedSyncPointClientState();

  ScopedSyncPointClientState(ScopedSyncPointClientState&& other);
  ScopedSyncPointClientState& operator=(ScopedSyncPointClientState&& other);

  explicit operator bool() const { return !!task_graph_; }

  // Explicitly destroys the corresponding SyncPointClientState, if the object
  // is valid.
  void Reset();

 private:
  raw_ptr<TaskGraph> task_graph_ = nullptr;
  SequenceId sequence_id_;
  CommandBufferNamespace namespace_id_ = CommandBufferNamespace::INVALID;
  CommandBufferId command_buffer_id_;
};

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

  static constexpr base::TimeDelta kMaxValidationDelay = base::Seconds(6);
  static constexpr base::TimeDelta kMinValidationDelay = base::Seconds(3);

  SequenceId CreateSequence(
      base::RepeatingClosure front_task_unblocked_callback,
      scoped_refptr<base::SingleThreadTaskRunner> validation_runner)
      LOCKS_EXCLUDED(lock_);

  SequenceId CreateSequence(
      base::RepeatingClosure front_task_unblocked_callback,
      scoped_refptr<base::SingleThreadTaskRunner> validation_runner,
      CommandBufferNamespace namespace_id,
      CommandBufferId command_buffer_id) LOCKS_EXCLUDED(lock_);

  void AddSequence(std::unique_ptr<Sequence> sequence) LOCKS_EXCLUDED(lock_);

  // Creates a SyncPointClientState object associated with the sequence.
  [[nodiscard]] ScopedSyncPointClientState CreateSyncPointClientState(
      SequenceId sequence_id,
      CommandBufferNamespace namespace_id,
      CommandBufferId command_buffer_id) LOCKS_EXCLUDED(lock_);

  void DestroySequence(SequenceId sequence_id) LOCKS_EXCLUDED(lock_);

  SyncPointManager* sync_point_manager() { return sync_point_manager_; }

  bool graph_validation_enabled() const {
    return sync_point_manager_->graph_validation_enabled();
  }

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
    //
    // If `namespace_id` is valid, also creates a SyncPointClientState that
    // lasts as long as the sequence itself.
    Sequence(
        TaskGraph* task_graph,
        base::RepeatingClosure front_task_unblocked_callback,
        scoped_refptr<base::SingleThreadTaskRunner> validation_runner,
        CommandBufferNamespace namespace_id = CommandBufferNamespace::INVALID,
        CommandBufferId command_buffer_id = {});

    Sequence(const Sequence&) = delete;
    Sequence& operator=(const Sequence&) = delete;

    virtual ~Sequence() EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    SequenceId sequence_id() const { return sequence_id_; }

    const scoped_refptr<SyncPointOrderData>& order_data() const {
      return order_data_;
    }

    // Creates a SyncPointClientState associated with the sequence. It is
    // destroyed either when the returned object is destructed or when the
    // sequence is destroyed, whichever happens earlier.
    [[nodiscard]] ScopedSyncPointClientState CreateSyncPointClientState(
        CommandBufferNamespace namespace_id,
        CommandBufferId command_buffer_id)
        EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    bool HasTasks() const EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_) {
      return !tasks_.empty();
    }

    // Enqueues a task in the sequence and returns the generated order number.
    uint32_t AddTask(TaskCallback task_callback,
                     std::vector<SyncToken> wait_fences,
                     const SyncToken& release,
                     ReportingCallback report_callback)
        EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    virtual uint32_t AddTask(base::OnceClosure task_closure,
                             std::vector<SyncToken> wait_fences,
                             const SyncToken& release,
                             ReportingCallback report_callback)
        EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    virtual uint32_t BeginTask(base::OnceClosure* task_closure)
        EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    // Should be called after running the closure returned by BeginTask().
    virtual void FinishTask() EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    // Continues running the current task with the given callback. Must be
    // called in between BeginTask() and FinishTask().
    void ContinueTask(TaskCallback task_callback)
        EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);
    virtual void ContinueTask(base::OnceClosure task_closure)
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

    const SyncToken& current_task_release() const
        EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_) {
      return current_task_release_;
    }

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
      Task(base::OnceClosure task_closure,
           uint32_t order_num,
           const SyncToken& release,
           ReportingCallback report_callback);
      ~Task();
      Task& operator=(Task&& other);

      // Always store tasks as closures. TaskCallbacks are bound with argument
      // and wrap as closures.
      base::OnceClosure task_closure;
      uint32_t order_num;
      SyncToken release;

      ReportingCallback report_callback;
      // Note: this time is only correct once the last fence has been removed,
      // as it is updated for all fences.
      base::TimeTicks running_ready = base::TimeTicks::Now();
      base::TimeTicks first_dependency_added;
      base::TimeTicks registration = base::TimeTicks::Now();

      // Records whether this task has been validated. Used if graph validation
      // is enabled.
      bool validated = false;
    };

    using TaskIter = typename base::circular_deque<Task>::iterator;

    // Must NOT be accessed under `&TaskGraph::lock_`.
    void Destroy() LOCKS_EXCLUDED(&TaskGraph::lock_);

    scoped_refptr<SyncPointClientState> TakeSyncPointClientState(
        CommandBufferNamespace namespace_id,
        CommandBufferId command_buffer_id)
        EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    void UpdateValidationTimer() EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    // Returns the range of wait fences for `task`.
    std::pair<WaitFenceConstIter, WaitFenceConstIter> GetTaskWaitFences(
        const Task& task) const EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    const Task* FindReleaseTask(const SyncToken& sync_token) const
        EXCLUSIVE_LOCKS_REQUIRED(&TaskGraph::lock_);

    base::OnceClosure CreateTaskClosure(TaskCallback task_callback);

    const raw_ptr<TaskGraph> task_graph_ = nullptr;
    const scoped_refptr<SyncPointOrderData> order_data_;
    const SequenceId sequence_id_;

    const base::RepeatingClosure front_task_unblocked_callback_
        GUARDED_BY(&TaskGraph::lock_);

    std::vector<scoped_refptr<SyncPointClientState>> sync_point_states_
        GUARDED_BY(&TaskGraph::lock_);

    // While processing a task, the task is removed from `tasks_`. This field is
    // used to preserve `release` of the task. So that it can be used in task
    // dependency validation; or if the task is later continued. In task
    // dependency validation, `current_task_release_` is treated as if it has
    // been released, because it no long has any precondition.
    SyncToken current_task_release_ GUARDED_BY(&TaskGraph::lock_);

    // Deque of tasks. Tasks are inserted at the back with increasing order
    // number generated from SyncPointOrderData. If a running task needs to be
    // continued, it is inserted at the front with the same order number.
    base::circular_deque<Task> tasks_ GUARDED_BY(&TaskGraph::lock_);

    // Set of fences that this sequence is waiting on. Fences are ordered in
    // increasing order number but may be removed out of order. Tasks are
    // blocked if there's a wait fence with order number less than or equal to
    // the task's order number.
    WaitFenceSet wait_fences_ GUARDED_BY(&TaskGraph::lock_);

    // Not supposed to be accessed from multiple thread simultaneously. It is
    // updated by BeginTask() and called by user task callback.
    FenceSyncReleaseDelegate release_delegate_;

    scoped_refptr<RetainingOneShotTimerHolder> validation_timer_;
  };

 private:
  friend class ScopedSyncPointClientState;

  // Records the validation state for a sequence.
  struct ValidateState {
    // Next task in the sequence to validate.
    Sequence::TaskIter next_to_validate;
    bool validating = false;
  };

  void SyncTokenFenceReleased(const SyncToken& sync_token,
                              uint32_t order_num,
                              SequenceId release_sequence_id,
                              SequenceId waiting_sequence_id)
      LOCKS_EXCLUDED(lock_);

  // Accessed by ScopedSyncPointClientState.
  void DestroySyncPointClientState(SequenceId sequence_id,
                                   CommandBufferNamespace namespace_id,
                                   CommandBufferId command_buffer_id)
      LOCKS_EXCLUDED(lock_);

  // Validates task dependencies, starting from `root_sequence`.
  void ValidateSequenceTaskFenceDeps(Sequence* root_sequence)
      LOCKS_EXCLUDED(lock_);

  using ReleaseMap = base::flat_map<SyncPointClientId, uint64_t>;

  // Note: GetSequenceValidateState() implementation requires a container that
  // doesn't invalidate references to existing elements on insertion. Therefore,
  // base::flat_map cannot be used.
  using ValidateStateMap = std::map<SequenceId, ValidateState>;

  // Validates dependencies for the task of `task_iter` on `sequence`.
  //
  // `pending_releases`: releases that are supposed to happen once the validated
  // tasks are executed.
  // `force_releases`: releases that need to be forcefully done to avoid invalid
  // waits.
  // `validate_states`: validation state of all the sequences.
  void ValidateTaskFenceDeps(Sequence* sequence,
                             Sequence::TaskIter task_iter,
                             ReleaseMap* pending_releases,
                             ReleaseMap* force_releases,
                             ValidateStateMap* validate_states)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Gets ValidateState for given `sequence`. It does necessary initialization
  // if the object hasn't been created yet, including updating
  // `pending_releases` with release of the currently ongoing task and validated
  // tasks.
  ValidateState& GetSequenceValidateState(ValidateStateMap* validate_states,
                                          ReleaseMap* pending_releases,
                                          Sequence* sequence)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  mutable base::Lock lock_;

  const raw_ptr<SyncPointManager> sync_point_manager_;

  base::flat_map<SequenceId, std::unique_ptr<Sequence>> sequence_map_
      GUARDED_BY(lock_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TASK_GRAPH_H_
