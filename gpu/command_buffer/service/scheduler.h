// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_H_

#include <queue>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/gpu_export.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gpu {
class SyncPointManager;
struct GpuPreferences;

// Forward-decl of the new DFS-based Scheduler.
class SchedulerDfs;

class GPU_EXPORT Scheduler {
  // A callback to be used for reporting when the task is ready to run (when the
  // dependencies have been solved).
  using ReportingCallback =
      base::OnceCallback<void(base::TimeTicks task_ready)>;

 public:
  struct GPU_EXPORT Task {
    Task(SequenceId sequence_id,
         base::OnceClosure closure,
         std::vector<SyncToken> sync_token_fences,
         ReportingCallback report_callback = ReportingCallback());
    Task(Task&& other);
    ~Task();
    Task& operator=(Task&& other);

    SequenceId sequence_id;
    base::OnceClosure closure;
    std::vector<SyncToken> sync_token_fences;
    ReportingCallback report_callback;
  };

  struct ScopedAddWaitingPriority {
   public:
    ScopedAddWaitingPriority(Scheduler* scheduler,
                             SequenceId sequence_id,
                             SchedulingPriority priority);
    ~ScopedAddWaitingPriority();

   private:
    const raw_ptr<Scheduler> scheduler_;
    const SequenceId sequence_id_;
    const SchedulingPriority priority_;
  };

  Scheduler(SyncPointManager* sync_point_manager,
            const GpuPreferences& gpu_preferences);

  Scheduler(const Scheduler&) = delete;
  Scheduler& operator=(const Scheduler&) = delete;

  ~Scheduler();

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

  // Raise priority of sequence for client wait (WaitForGetOffset/TokenInRange)
  // on given command buffer.
  void RaisePriorityForClientWait(SequenceId sequence_id,
                                  CommandBufferId command_buffer_id);

  // Reset priority of sequence if it was increased for a client wait.
  void ResetPriorityForClientWait(SequenceId sequence_id,
                                  CommandBufferId command_buffer_id);

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

  // Returns pointer to a SchedulerDfs instance if the feature flag
  // kUseGpuSchedulerDfs is enabled. Otherwise returns null. Used in unit test
  // to directly test methods that exist only in SchedulerDfs.
  SchedulerDfs* GetSchedulerDfsForTesting() { return scheduler_dfs_.get(); }

 private:
  struct SchedulingState {
    static bool Comparator(const SchedulingState& lhs,
                           const SchedulingState& rhs) {
      return rhs.RunsBefore(lhs);
    }

    SchedulingState();
    SchedulingState(const SchedulingState& other);
    ~SchedulingState();

    bool RunsBefore(const SchedulingState& other) const {
      return std::tie(priority, order_num) <
             std::tie(other.priority, other.order_num);
    }

    void WriteIntoTrace(perfetto::TracedValue context) const;

    SequenceId sequence_id;
    SchedulingPriority priority = SchedulingPriority::kLow;
    uint32_t order_num = 0;
  };

  class GPU_EXPORT Sequence {
   public:
    Sequence(Scheduler* scheduler,
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

    // The sequence is runnable if its enabled and has tasks which are not
    // blocked by wait fences.
    bool IsRunnable() const;

    // Returns true if this sequence's scheduling state changed and it needs to
    // be reinserted into the scheduling queue.
    bool NeedsRescheduling() const;

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
    // state to IDLE.
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

    void AddClientWait(CommandBufferId command_buffer_id);

    void RemoveClientWait(CommandBufferId command_buffer_id);

    SchedulingPriority current_priority() const { return current_priority_; }

   private:
    friend class Scheduler;

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

    // Description of Stream priority propagation: Each Stream has an initial
    // priority ('default_priority_').  When a Stream has other Streams waiting
    // on it via a 'WaitFence', it computes it's own priority based on those
    // fences, by keeping count of the priority of each incoming WaitFence's
    // priority in 'waiting_priority_counts_'.
    //
    // 'wait_fences_' maps each 'WaitFence' to it's current priority.  Initially
    // WaitFences take the priority of the waiting Stream, and propagate their
    // priority to the releasing Stream via AddWaitingPriority().
    //
    // A higher priority waiting stream or ClientWait, can recursively pass on
    // it's priority to existing 'ClientWaits' via PropagatePriority(), which
    // updates the releasing stream via ChangeWaitingPriority().
    //
    // When a 'WaitFence' is removed either by the SyncToken being released,
    // or when the waiting Stream is Destroyed, it removes it's priority from
    // the releasing stream via RemoveWaitingPriority().

    // Propagate a priority to all wait fences.
    void PropagatePriority(SchedulingPriority priority);

    // Add a waiting priority.
    void AddWaitingPriority(SchedulingPriority priority);

    // Remove a waiting priority.
    void RemoveWaitingPriority(SchedulingPriority priority);

    // Change a waiting priority.
    void ChangeWaitingPriority(SchedulingPriority old_priority,
                               SchedulingPriority new_priority);

    // Re-compute current priority.
    void UpdateSchedulingPriority();

    // If the sequence is enabled. Sequences are disabled/enabled based on when
    // the command buffer is descheduled/scheduled.
    bool enabled_ = true;

    RunningState running_state_ = IDLE;

    // Cached scheduling state used for comparison with other sequences while
    // running. Updated in |SetScheduled| and |UpdateRunningPriority|.
    SchedulingState scheduling_state_;

    const raw_ptr<Scheduler> scheduler_;
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

    // Counts of pending releases bucketed by scheduling priority.
    int waiting_priority_counts_[static_cast<int>(SchedulingPriority::kLast) +
                                 1] = {};

    base::flat_set<CommandBufferId> client_waits_;
  };

  void AddWaitingPriority(SequenceId sequence_id, SchedulingPriority priority);
  void RemoveWaitingPriority(SequenceId sequence_id,
                             SchedulingPriority priority);

  void SyncTokenFenceReleased(const SyncToken& sync_token,
                              uint32_t order_num,
                              SequenceId release_sequence_id,
                              SequenceId waiting_sequence_id);

  void ScheduleTaskHelper(Scheduler::Task task);

  void TryScheduleSequence(Sequence* sequence);

  // If the scheduling queue needs to be rebuild because a sequence changed
  // priority.
  std::vector<SchedulingState>& RebuildSchedulingQueueIfNeeded(
      base::SingleThreadTaskRunner* task_runner);

  Sequence* GetSequence(SequenceId sequence_id);

  void RunNextTask();

  const raw_ptr<SyncPointManager> sync_point_manager_;
  mutable base::Lock lock_;
  base::flat_map<SequenceId, std::unique_ptr<Sequence>> sequence_map_
      GUARDED_BY(lock_);

  // Each thread will have its own priority queue to schedule sequences
  // created on that thread.
  struct PerThreadState {
    PerThreadState();
    PerThreadState(PerThreadState&&);
    ~PerThreadState();
    PerThreadState& operator=(PerThreadState&&);

    // Used as a priority queue for scheduling sequences. Min heap of
    // SchedulingState with highest priority (lowest order) in front.
    std::vector<SchedulingState> scheduling_queue;

    // Indicates if the scheduling queue for this thread should be rebuilt due
    // to priority changes, sequences becoming unblocked, etc.
    bool rebuild_scheduling_queue = false;

    // Indicates if the scheduler is actively running tasks on this thread.
    bool running = false;

    // Indicates when the next task run was scheduled
    base::TimeTicks run_next_task_scheduled;
  };
  base::flat_map<base::SingleThreadTaskRunner*, PerThreadState>
      per_thread_state_map_ GUARDED_BY(lock_);

  // A pointer to a SchedulerDfs instance. If set, all public SchedulerDfs
  // methods are forwarded to this SchedulerDfs instance. |scheduler_dfs_| is
  // set depending on a Finch experimental feature.
  std::unique_ptr<SchedulerDfs> scheduler_dfs_;

 private:
  base::MetricsSubSampler metrics_subsampler_;

  FRIEND_TEST_ALL_PREFIXES(SchedulerTest, StreamPriorities);
  FRIEND_TEST_ALL_PREFIXES(SchedulerTest, StreamDestroyRemovesPriorities);
  FRIEND_TEST_ALL_PREFIXES(SchedulerTest, StreamPriorityChangeWhileReleasing);
  FRIEND_TEST_ALL_PREFIXES(SchedulerTest, CircularPriorities);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_H_
