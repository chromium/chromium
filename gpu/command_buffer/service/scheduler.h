// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_H_

#include <queue>
#include <vector>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/gpu_export.h"

namespace base {
class SingleThreadTaskRunner;
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace gpu {
class SyncPointManager;
struct GpuPreferences;

class GPU_EXPORT Scheduler {
 public:
  struct GPU_EXPORT Task {
    Task(SequenceId sequence_id,
         base::OnceClosure closure,
         std::vector<SyncToken> sync_token_fences);
    Task(Task&& other);
    ~Task();
    Task& operator=(Task&& other);

    SequenceId sequence_id;
    base::OnceClosure closure;
    std::vector<SyncToken> sync_token_fences;
  };

  Scheduler(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
            SyncPointManager* sync_point_manager,
            const GpuPreferences& gpu_preferences);

  virtual ~Scheduler();

  // Create a sequence with given priority. Returns an identifier for the
  // sequence that can be used with SyncPonintManager for creating sync point
  // release clients. Sequences start off as enabled (see |EnableSequence|).
  // Sequence could be created outside of GPU thread.
  SequenceId CreateSequence(SchedulingPriority priority);

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
  void ScheduleTask(Task task);

  void ScheduleTasks(std::vector<Task> tasks);

  // Continue running task on the sequence with the closure. This must be called
  // while running a previously scheduled task.
  void ContinueTask(SequenceId sequence_id, base::OnceClosure closure);

  // If the sequence should yield so that a higher priority sequence may run.
  bool ShouldYield(SequenceId sequence_id);

  base::WeakPtr<Scheduler> AsWeakPtr();

  // Takes and resets current accumulated blocking time. Not available on all
  // platforms. Must be enabled with --enable-gpu-blocked-time.
  // Returns TimeDelta::Min() when not available.
  base::TimeDelta TakeTotalBlockingTime();

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

    std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValue()
        const;

    SequenceId sequence_id;
    SchedulingPriority priority = SchedulingPriority::kLow;
    uint32_t order_num = 0;
  };

  class GPU_EXPORT Sequence {
   public:
    Sequence(Scheduler* scheduler,
             SequenceId sequence_id,
             SchedulingPriority priority,
             scoped_refptr<SyncPointOrderData> order_data);

    ~Sequence();

    SequenceId sequence_id() const { return sequence_id_; }

    const scoped_refptr<SyncPointOrderData>& order_data() const {
      return order_data_;
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

    // Returns the next order number and closure. Sets running state to RUNNING.
    uint32_t BeginTask(base::OnceClosure* closure);

    // Called after running the closure returned by BeginTask. Sets running
    // state to IDLE.
    void FinishTask();

    // Enqueues a task in the sequence and returns the generated order number.
    uint32_t ScheduleTask(base::OnceClosure closure);

    // Continue running the current task with the given closure. Must be called
    // in between |BeginTask| and |FinishTask|.
    void ContinueTask(base::OnceClosure closure);

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
      Task(base::OnceClosure closure, uint32_t order_num);
      ~Task();
      Task& operator=(Task&& other);

      base::OnceClosure closure;
      uint32_t order_num;
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

    Scheduler* const scheduler_;
    const SequenceId sequence_id_;

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

    DISALLOW_COPY_AND_ASSIGN(Sequence);
  };

  void SyncTokenFenceReleased(const SyncToken& sync_token,
                              uint32_t order_num,
                              SequenceId release_sequence_id,
                              SequenceId waiting_sequence_id);

  void ScheduleTaskHelper(Task task);

  void TryScheduleSequence(Sequence* sequence);

  void RebuildSchedulingQueue();

  Sequence* GetSequence(SequenceId sequence_id);

  void RunNextTask();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  SyncPointManager* const sync_point_manager_;

  mutable base::Lock lock_;

  // The following are protected by |lock_|.
  bool running_ = false;

  base::flat_map<SequenceId, std::unique_ptr<Sequence>> sequences_;

  // Used as a priority queue for scheduling sequences. Min heap of
  // SchedulingState with highest priority (lowest order) in front.
  std::vector<SchedulingState> scheduling_queue_;

  // If the scheduling queue needs to be rebuild because a sequence changed
  // priority.
  bool rebuild_scheduling_queue_ = false;

  // Accumulated time the thread was blocked during running task
  base::TimeDelta total_blocked_time_;
  const bool blocked_time_collection_enabled_;

  base::ThreadChecker thread_checker_;

  // Invalidated on main thread.
  base::WeakPtr<Scheduler> weak_ptr_;
  base::WeakPtrFactory<Scheduler> weak_factory_{this};

 private:
  FRIEND_TEST_ALL_PREFIXES(SchedulerTest, StreamPriorities);
  FRIEND_TEST_ALL_PREFIXES(SchedulerTest, StreamDestroyRemovesPriorities);
  FRIEND_TEST_ALL_PREFIXES(SchedulerTest, StreamPriorityChangeWhileReleasing);
  FRIEND_TEST_ALL_PREFIXES(SchedulerTest, CircularPriorities);
  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_H_
