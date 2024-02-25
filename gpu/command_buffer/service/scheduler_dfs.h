// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_DFS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_DFS_H_

#include <queue>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/gpu_export.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gpu {
class SyncPointManager;
struct GpuPreferences;

class GPU_EXPORT SchedulerDfs {
  // A callback to be used for reporting when the task is ready to run (when the
  // dependencies have been solved).
  using ReportingCallback =
      base::OnceCallback<void(base::TimeTicks task_ready)>;

 public:
  SchedulerDfs(SyncPointManager* sync_point_manager,
               const GpuPreferences& gpu_preferences);

  SchedulerDfs(const SchedulerDfs&) = delete;
  SchedulerDfs& operator=(const SchedulerDfs&) = delete;

  ~SchedulerDfs();

  // Create a sequence with given priority. Returns an identifier for the
  // sequence that can be used with SyncPointManager for creating sync point
  // release clients. Sequences start off as enabled (see |EnableSequence|).
  // Sequence is bound to the provided |task_runner|.
  SequenceId CreateSequence(
      SchedulingPriority priority,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Should be only used for tests.
  SequenceId CreateSequenceForTesting(SchedulingPriority priority);

  // Destroy the sequence and run any scheduled tasks immediately. Sequence
  // could be destroyed outside of GPU thread.
  void DestroySequence(SequenceId sequence_id);

  // Enables the sequence so that its tasks may be scheduled.
  void EnableSequence(SequenceId sequence_id);

  // Disables the sequence.
  void DisableSequence(SequenceId sequence_id);

  // Gets the priority that the sequence was created with.
  SchedulingPriority GetSequenceDefaultPriority(SequenceId sequence_id);

  // Changes a sequence's priority. Used in WaitForGetOffset/TokenInRange to
  // temporarily increase a sequence's priority.
  void SetSequencePriority(SequenceId sequence_id, SchedulingPriority priority);

  // Schedules task (closure) to run on the sequence. The task is blocked until
  // the sync token fences are released or determined to be invalid. Tasks are
  // run in the order in which they are submitted.
  void ScheduleTask(Scheduler::Task task);

  void ScheduleTasks(std::vector<Scheduler::Task> tasks);

  // Continue running task on the sequence with the closure. This must be called
  // while running a previously scheduled task.
  void ContinueTask(SequenceId sequence_id, base::OnceClosure closure);

  // If the sequence should yield so that a higher priority sequence may run.
  bool ShouldYield(SequenceId sequence_id);

  base::SingleThreadTaskRunner* GetTaskRunnerForTesting(SequenceId sequence_id);

 private:
  struct SchedulingState {
    SchedulingState();
    SchedulingState(const SchedulingState& other);
    ~SchedulingState();

    static bool RunsBefore(const SchedulingState& lhs,
                           const SchedulingState& rhs) {
      return std::tie(lhs.priority, lhs.order_num) <
             std::tie(rhs.priority, rhs.order_num);
    }

    void WriteIntoTrace(perfetto::TracedValue context) const;

    bool operator==(const SchedulingState& rhs) const;

    SequenceId sequence_id;
    SchedulingPriority priority = SchedulingPriority::kLow;
    uint32_t order_num = 0;
  };

  class GPU_EXPORT Sequence {
   public:
    Sequence(SchedulerDfs* scheduler,
             SequenceId sequence_id,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner,
             SchedulingPriority priority,
             scoped_refptr<SyncPointOrderData> order_data);

    Sequence(const Sequence&) = delete;
    Sequence& operator=(const Sequence&) = delete;

    ~Sequence();

    SequenceId sequence_id() const { return sequence_id_; }

    const scoped_refptr<SyncPointOrderData>& order_data() const {
      return order_data_;
    }

    base::SingleThreadTaskRunner* task_runner() const {
      return task_runner_.get();
    }

    bool enabled() const { return enabled_; }

    bool scheduled() const { return running_state_ == SCHEDULED; }

    bool running() const { return running_state_ == RUNNING; }

    bool HasTasks() const;

    // A sequence is runnable if it is enabled, is not already running, and has
    // tasks in its queue. Note that this does *not* necessarily mean that its
    // first task unblocked.
    bool IsRunnable() const { return enabled() && !running() && HasTasks(); }

    // Returns true if this sequence should yield to another sequence. Uses the
    // cached scheduling state for comparison.
    bool ShouldYieldTo(const Sequence* other) const;

    // Enables or disables the sequence.
    void SetEnabled(bool enabled);

    // Sets running state to SCHEDULED. Returns scheduling state for this
    // sequence used for inserting in the scheduling queue.
    SchedulingState SetScheduled();

    // Update cached scheduling priority while running.
    void UpdateRunningPriority();

    // The time delta it took for the front task's dependencies to be completed.
    base::TimeDelta FrontTaskWaitingDependencyDelta();

    // The delay between when the front task was ready to run (no more
    // dependencies) and now. This is used when the task is actually started to
    // check for low scheduling delays.
    base::TimeDelta FrontTaskSchedulingDelay();

    // Returns the next order number and closure. Sets running state to RUNNING.
    uint32_t BeginTask(base::OnceClosure* closure);

    // Called after running the closure returned by BeginTask. Sets running
    // state to SCHEDULED.
    void FinishTask();

    // Enqueues a task in the sequence and returns the generated order number.
    uint32_t ScheduleTask(base::OnceClosure closure,
                          ReportingCallback report_callback);

    // Continue running the current task with the given closure. Must be called
    // in between |BeginTask| and |FinishTask|.
    void ContinueTask(base::OnceClosure closure);

    // Sets the first dependency added time on the last task if it wasn't
    // already set, no-op otherwise.
    void SetLastTaskFirstDependencyTimeIfNeeded();

    // Add a sync token fence that this sequence should wait on.
    void AddWaitFence(const SyncToken& sync_token,
                      uint32_t order_num,
                      SequenceId release_sequence_id);

    // Remove a waiting sync token fence.
    void RemoveWaitFence(const SyncToken& sync_token,
                         uint32_t order_num,
                         SequenceId release_sequence_id);

    SchedulingPriority current_priority() const { return current_priority_; }

   private:
    friend class SchedulerDfs;

    enum RunningState { IDLE, SCHEDULED, RUNNING };

    struct WaitFence {
      WaitFence(WaitFence&& other);
      WaitFence(const SyncToken& sync_token,
                uint32_t order_num,
                SequenceId release_sequence_id);
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
    };

    // Returns true if the sequence is not empty, and the first task does not
    // have any pending dependencies.
    bool IsNextTaskUnblocked() const;

    // If the sequence is enabled. Sequences are disabled/enabled based on when
    // the command buffer is descheduled/scheduled.
    bool enabled_ = true;

    // TODO(elgarawany): This is no longer needed. Replace with bool running_.
    RunningState running_state_ = IDLE;

    // Cached scheduling state used for comparison with other sequences while
    // running. Updated in |SetScheduled| and |UpdateRunningPriority|.
    SchedulingState scheduling_state_;

    const raw_ptr<SchedulerDfs> scheduler_;
    const SequenceId sequence_id_;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

    const SchedulingPriority default_priority_;
    SchedulingPriority current_priority_;

    scoped_refptr<SyncPointOrderData> order_data_;

    // Deque of tasks. Tasks are inserted at the back with increasing order
    // number generated from SyncPointOrderData. If a running task needs to be
    // continued, it is inserted at the front with the same order number.
    base::circular_deque<Task> tasks_;

    // Map of fences that this sequence is waiting on. Fences are ordered in
    // increasing order number but may be removed out of order. Tasks are
    // blocked if there's a wait fence with order number less than or equal to
    // the task's order number.
    base::flat_map<WaitFence, SchedulingPriority> wait_fences_;
  };

  Sequence* GetSequence(SequenceId sequence_id);

  void SyncTokenFenceReleased(const SyncToken& sync_token,
                              uint32_t order_num,
                              SequenceId release_sequence_id,
                              SequenceId waiting_sequence_id);

  void ScheduleTaskHelper(Scheduler::Task task) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void TryScheduleSequence(Sequence* sequence);

  // Returns a sorted list of runnable sequences.
  const std::vector<SchedulingState>& GetSortedRunnableSequences(
      base::SingleThreadTaskRunner* task_runner)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns true if there are *any* unblocked tasks in sequences assigned to
  // |task_runner|. This is used to decide if RunNextTask needs to be
  // rescheduled.
  bool HasAnyUnblockedTasksOnRunner(
      const base::SingleThreadTaskRunner* task_runner) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Finds the sequence of the next task that can be run under |root_sequence|'s
  // dependency graph. This function will visit sequences that are tied to other
  // threads in case other threads depends on work on this thread; however, it
  // will not run any *leaves* that are tied to other threads. nullptr is
  // returned only if DrDC is enabled and when a sequence's only dependency is a
  // sequence that is tied to another thread.
  Sequence* FindNextTaskFromRoot(Sequence* root_sequence)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Calls |FindNextTaskFromRoot| on the ordered list of all sequences, and
  // returns the first runnable task. Returns nullptr if there is no work to do.
  Sequence* FindNextTask() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Executes the closure of the first task for |sequence_id|. Assumes that
  // the sequence has tasks, and that the first task is unblocked.
  void ExecuteSequence(SequenceId sequence_id) LOCKS_EXCLUDED(lock_);

  // The scheduler's main loop. At each tick, it will call FindNextTask on the
  // sorted list of sequences to find the highest priority available sequence.
  // The scheduler then walks the sequence's dependency graph using DFS to find
  // any unblocked task. If there are multiple choices at any node, the
  // scheduler picks the dependency with the lowest SchedulingState, effectively
  // ordering the dependencies by priority and order_num.
  void RunNextTask() LOCKS_EXCLUDED(lock_);

  mutable base::Lock lock_;

  const raw_ptr<SyncPointManager> sync_point_manager_;

  base::flat_map<SequenceId, std::unique_ptr<Sequence>> sequence_map_
      GUARDED_BY(lock_);

  base::MetricsSubSampler metrics_subsampler_ GUARDED_BY(lock_);

  // Each thread will have its own priority queue to schedule sequences
  // created on that thread.
  struct PerThreadState {
    PerThreadState();
    PerThreadState(PerThreadState&&);
    ~PerThreadState();
    PerThreadState& operator=(PerThreadState&&);

    // Sorted list of SchedulingState that contains sequences that Runnable. Is
    // only used so that GetSortedRunnableSequences does not have to re-allocate
    // a vector. It is rebuilt at each call to GetSortedRunnableSequences.
    std::vector<SchedulingState> sorted_sequences;

    // Indicates if the scheduler is actively running tasks on this thread.
    bool running = false;

    // Indicates when the next task run was scheduled
    base::TimeTicks run_next_task_scheduled;
  };
  base::flat_map<base::SingleThreadTaskRunner*, PerThreadState>
      per_thread_state_map_ GUARDED_BY(lock_);

  // Accumulated time the thread was blocked during running task
  base::TimeDelta total_blocked_time_ GUARDED_BY(lock_);

 private:
  FRIEND_TEST_ALL_PREFIXES(SchedulerDfsTest, StreamPriorities);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_H_
