// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/gpu_command_buffer_service_export.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/command_buffer/service/task_graph.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace gpu {

class SchedulerTaskRunner;

class GPU_COMMAND_BUFFER_SERVICE_EXPORT Scheduler {
 public:
  struct GPU_COMMAND_BUFFER_SERVICE_EXPORT Task {
    // Use the signature with TaskCallback if the task needs to determine when
    // to release fence sync during task execution. Please also see comments of
    // TaskCallback.
    // Use the signatures with base::OnceClosure if the task doesn't release
    // fence sync, or the release can be done automatically after task
    // execution.
    Task(SequenceId sequence_id,
         TaskCallback task_callback,
         std::vector<SyncToken> sync_token_fences,
         const SyncToken& release,
         ReportingCallback report_callback = ReportingCallback());

    Task(SequenceId sequence_id,
         base::OnceClosure task_closure,
         std::vector<SyncToken> sync_token_fences,
         const SyncToken& release,
         ReportingCallback report_callback = ReportingCallback());

    Task(SequenceId sequence_id,
         base::OnceClosure task_closure,
         std::vector<SyncToken> sync_token_fences = {},
         ReportingCallback report_callback = ReportingCallback());

    Task(Task&& other);
    ~Task();
    Task& operator=(Task&& other);

    SequenceId sequence_id;

    // Only one of the two is used.
    TaskCallback task_callback;
    base::OnceClosure task_closure;

    std::vector<SyncToken> sync_token_fences;

    // The release that is expected to be reached after execution of this task.
    SyncToken release;

    ReportingCallback report_callback;
  };

  struct GPU_COMMAND_BUFFER_SERVICE_EXPORT ScopedSetSequencePriority {
   public:
    ScopedSetSequencePriority(Scheduler* scheduler,
                              SequenceId sequence_id,
                              SchedulingPriority priority);
    ~ScopedSetSequencePriority();

   private:
    const raw_ptr<Scheduler> scheduler_;
    const SequenceId sequence_id_;
  };

  // Creates the Scheduler implementation.
  static std::unique_ptr<Scheduler> Create(
      SyncPointManager* sync_point_manager);

  Scheduler(const Scheduler&) = delete;
  Scheduler& operator=(const Scheduler&) = delete;

  virtual ~Scheduler() LOCKS_EXCLUDED(lock());

  // Create a sequence with given priority. Returns an identifier for the
  // sequence that can be used with SyncPointManager for creating sync point
  // release clients. Sequences start off as enabled (see |EnableSequence|).
  // Sequence is bound to the provided |task_runner|.
  SequenceId CreateSequence(
      SchedulingPriority priority,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      LOCKS_EXCLUDED(lock());

  // Similar to the method above, but also creates a SyncPointClientState
  // associated with the sequence. The SyncPointClientState object is destroyed
  // when the sequence is destroyed.
  virtual SequenceId CreateSequence(
      SchedulingPriority priority,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      CommandBufferNamespace namespace_id,
      CommandBufferId command_buffer_id) LOCKS_EXCLUDED(lock()) = 0;

  // Destroy the sequence and run any scheduled tasks immediately. Sequence
  // could be destroyed outside of GPU thread.
  virtual void DestroySequence(SequenceId sequence_id)
      LOCKS_EXCLUDED(lock()) = 0;

  [[nodiscard]] ScopedSyncPointClientState CreateSyncPointClientState(
      SequenceId sequence_id,
      CommandBufferNamespace namespace_id,
      CommandBufferId command_buffer_id) LOCKS_EXCLUDED(lock());

  // Enables the sequence so that its tasks may be scheduled.
  void EnableSequence(SequenceId sequence_id) LOCKS_EXCLUDED(lock());

  // Disables the sequence.
  void DisableSequence(SequenceId sequence_id) LOCKS_EXCLUDED(lock());

  // Gets the priority that the sequence was created with.
  SchedulingPriority GetSequenceDefaultPriority(SequenceId sequence_id)
      LOCKS_EXCLUDED(lock());

  // Gets the current priority of the sequence.
  SchedulingPriority GetSequencePriority(SequenceId sequence_id)
      LOCKS_EXCLUDED(lock());

  // Changes a sequence's priority. Used in WaitForGetOffset/TokenInRange to
  // temporarily increase a sequence's priority.
  void SetSequencePriority(SequenceId sequence_id, SchedulingPriority priority)
      LOCKS_EXCLUDED(lock());

  // Returns a task runner that schedules posted tasks on the sequence. It is
  // shared by all users of the sequence and shut down by DestroySequence(),
  // after which posting to it fails.
  virtual scoped_refptr<base::SequencedTaskRunner> GetTaskRunner(
      SequenceId sequence_id) LOCKS_EXCLUDED(lock()) = 0;

  // Schedules task to run on the sequence. The task is blocked until the sync
  // token fences are released or determined to be invalid. Tasks are run in the
  // order in which they are submitted.
  void ScheduleTask(Scheduler::Task task) LOCKS_EXCLUDED(lock());
  void ScheduleTask(SequenceId sequence_id, base::OnceClosure task_closure)
      LOCKS_EXCLUDED(lock());

  void ScheduleTasks(std::vector<Scheduler::Task> tasks) LOCKS_EXCLUDED(lock());

  // Continue running task on the sequence with the callback. This must be
  // called while running a previously scheduled task.
  void ContinueTask(SequenceId sequence_id, TaskCallback task_callback)
      LOCKS_EXCLUDED(lock());
  void ContinueTask(SequenceId sequence_id, base::OnceClosure task_closure)
      LOCKS_EXCLUDED(lock());

  // If the sequence should yield so that a higher priority sequence may run.
  virtual bool ShouldYield(SequenceId sequence_id) LOCKS_EXCLUDED(lock()) = 0;

  bool graph_validation_enabled() const {
    return task_graph_.graph_validation_enabled();
  }

  TaskGraph* task_graph() { return &task_graph_; }

 protected:
  // All public methods except constructor must be accessed under TaskGraph's
  // lock. Please see locking annotation of individual methods.
  class GPU_COMMAND_BUFFER_SERVICE_EXPORT Sequence
      : public TaskGraph::Sequence {
   public:
    Sequence(
        TaskGraph* task_graph,
        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
        SchedulingPriority priority,
        CommandBufferNamespace namespace_id = CommandBufferNamespace::INVALID,
        CommandBufferId command_buffer_id = {});

    Sequence(const Sequence&) = delete;
    Sequence& operator=(const Sequence&) = delete;

    ~Sequence() override EXCLUSIVE_LOCKS_REQUIRED(lock());

    base::SingleThreadTaskRunner* task_runner() const {
      return task_runner_.get();
    }

    bool enabled() const EXCLUSIVE_LOCKS_REQUIRED(lock()) { return enabled_; }

    SchedulingPriority default_priority() const { return default_priority_; }

    SchedulingPriority current_priority() const
        EXCLUSIVE_LOCKS_REQUIRED(lock()) {
      return current_priority_;
    }

   protected:
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

   private:
    friend class Scheduler;

    // Enables or disables the sequence. Returns true if the enabled state
    // changed.
    bool SetEnabled(bool enabled) EXCLUSIVE_LOCKS_REQUIRED(lock());

    // If the sequence is enabled. Sequences are disabled/enabled based on when
    // the command buffer is descheduled/scheduled.
    bool enabled_ GUARDED_BY(lock()) = true;

    const SchedulingPriority default_priority_;
    SchedulingPriority current_priority_ GUARDED_BY(lock());
  };

  explicit Scheduler(SyncPointManager* sync_point_manager);

  base::Lock& lock() const LOCK_RETURNED(task_graph_.lock()) {
    return task_graph_.lock();
  }

  Sequence* GetSequence(SequenceId sequence_id)
      EXCLUSIVE_LOCKS_REQUIRED(lock());

  virtual void ScheduleTaskLocked(Scheduler::Task task)
      EXCLUSIVE_LOCKS_REQUIRED(lock()) = 0;

  virtual void OnSequenceEnabledChanged(Sequence* sequence)
      EXCLUSIVE_LOCKS_REQUIRED(lock()) = 0;

  virtual void OnSequencePriorityChanged() EXCLUSIVE_LOCKS_REQUIRED(lock()) {}

  TaskGraph task_graph_;

  // The Sequence instances in the map are owned by `task_graph_`.
  base::flat_map<SequenceId, Sequence*> scheduler_sequence_map_
      GUARDED_BY(lock());

 private:
  FRIEND_TEST_ALL_PREFIXES(SchedulerTest, StreamPriorities);
};

// The gpu::Scheduler implementation that runs tasks from a per-thread
// RunNextTask() loop, picking the next task with a priority-ordered DFS over
// the sequences' sync token dependencies.
class GPU_COMMAND_BUFFER_SERVICE_EXPORT SchedulerImpl : public Scheduler {
 public:
  explicit SchedulerImpl(SyncPointManager* sync_point_manager);

  SchedulerImpl(const SchedulerImpl&) = delete;
  SchedulerImpl& operator=(const SchedulerImpl&) = delete;

  ~SchedulerImpl() override LOCKS_EXCLUDED(lock());

  // Scheduler:
  using Scheduler::CreateSequence;
  SequenceId CreateSequence(
      SchedulingPriority priority,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      CommandBufferNamespace namespace_id,
      CommandBufferId command_buffer_id) override LOCKS_EXCLUDED(lock());
  void DestroySequence(SequenceId sequence_id) override LOCKS_EXCLUDED(lock());
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner(
      SequenceId sequence_id) override LOCKS_EXCLUDED(lock());
  bool ShouldYield(SequenceId sequence_id) override LOCKS_EXCLUDED(lock());

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
    uint64_t order_num = 0;
  };

  // All public methods except constructor must be accessed under TaskGraph's
  // lock. Please see locking annotation of individual methods.
  class GPU_COMMAND_BUFFER_SERVICE_EXPORT Sequence
      : public Scheduler::Sequence {
   public:
    Sequence(
        SchedulerImpl* scheduler,
        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
        SchedulingPriority priority,
        CommandBufferNamespace namespace_id = CommandBufferNamespace::INVALID,
        CommandBufferId command_buffer_id = {});

    Sequence(const Sequence&) = delete;
    Sequence& operator=(const Sequence&) = delete;

    ~Sequence() override EXCLUSIVE_LOCKS_REQUIRED(lock());

    bool scheduled() const EXCLUSIVE_LOCKS_REQUIRED(lock()) {
      return running_state_ == SCHEDULED;
    }

    bool running() const EXCLUSIVE_LOCKS_REQUIRED(lock()) {
      return running_state_ == RUNNING;
    }

    bool HasTasksAndEnabled() const EXCLUSIVE_LOCKS_REQUIRED(lock()) {
      return enabled() && HasTasks();
    }

    // A sequence is runnable if it is enabled, is not already running, and has
    // tasks in its queue. Note that this does *not* necessarily mean that its
    // first task unblocked.
    bool IsRunnable() const EXCLUSIVE_LOCKS_REQUIRED(lock()) {
      return enabled() && !running() && HasTasks();
    }

    // Returns true if this sequence should yield to another sequence. Uses the
    // cached scheduling state for comparison.
    bool ShouldYieldTo(const Sequence* other) const
        EXCLUSIVE_LOCKS_REQUIRED(lock());

    // Sets running state to SCHEDULED. Returns scheduling state for this
    // sequence used for inserting in the scheduling queue.
    SchedulingState SetScheduled() EXCLUSIVE_LOCKS_REQUIRED(lock());

    // Update cached scheduling priority while running.
    void UpdateRunningPriority() EXCLUSIVE_LOCKS_REQUIRED(lock());

    using TaskGraph::Sequence::AddTask;

    // Returns the next order number and closure. Sets running state to RUNNING.
    uint64_t BeginTask(base::OnceClosure* task_closure) override
        EXCLUSIVE_LOCKS_REQUIRED(lock());

    // Called after running the closure returned by BeginTask. Sets running
    // state to SCHEDULED.
    void FinishTask() override EXCLUSIVE_LOCKS_REQUIRED(lock());

    using TaskGraph::Sequence::ContinueTask;

    // Continue running the current task with the given closure. Must be called
    // in between |BeginTask| and |FinishTask|.
    void ContinueTask(base::OnceClosure task_closure) override
        EXCLUSIVE_LOCKS_REQUIRED(lock());

    void OnFrontTaskUnblocked(uint64_t order_num) override
        EXCLUSIVE_LOCKS_REQUIRED(lock());

   private:
    friend class SchedulerImpl;

    enum RunningState { IDLE, SCHEDULED, RUNNING };

    // TODO(elgarawany): This is no longer needed. Replace with bool running_.
    RunningState running_state_ GUARDED_BY(lock()) = IDLE;

    // Cached scheduling state used for comparison with other sequences while
    // running. Updated in |SetScheduled| and |UpdateRunningPriority|.
    SchedulingState scheduling_state_ GUARDED_BY(lock());

    // Uses UnprotectedInRelease for performance reasons: no release-build
    // overhead. See raw_ptr.h for more info.
    const raw_ptr<SchedulerImpl, UnprotectedInRelease> scheduler_ = nullptr;

    // Created lazily by SchedulerImpl::GetTaskRunner() and shut down in
    // DestroySequence().
    scoped_refptr<SchedulerTaskRunner> scheduler_task_runner_
        GUARDED_BY(lock());
  };

  // Each thread will have its own priority queue to schedule sequences
  // created on that thread.
  struct PerThreadState {
    PerThreadState();
    PerThreadState(PerThreadState&&);
    ~PerThreadState();
    PerThreadState& operator=(PerThreadState&&);

    // Keep track of the number of sequences that post tasks to this thread, to
    // determine when map entries can be pruned.
    int num_sequences = 0;

    // Sorted list of SchedulingState that contains sequences that Runnable. Is
    // only used so that GetSortedRunnableSequences does not have to re-allocate
    // a vector. It is rebuilt at each call to GetSortedRunnableSequences.
    std::vector<SchedulingState> sorted_sequences;

    // Indicates if the scheduler is actively running tasks on this thread.
    bool running = false;

    // Indicates when the next task run was scheduled
    base::TimeTicks run_next_task_scheduled;
  };

  // Scheduler:
  void ScheduleTaskLocked(Scheduler::Task task) override
      EXCLUSIVE_LOCKS_REQUIRED(lock());
  void OnSequenceEnabledChanged(Scheduler::Sequence* sequence) override
      EXCLUSIVE_LOCKS_REQUIRED(lock());

  Sequence* GetSequence(SequenceId sequence_id)
      EXCLUSIVE_LOCKS_REQUIRED(lock());

  PerThreadState& GetThreadState(base::SingleThreadTaskRunner* task_runner)
      EXCLUSIVE_LOCKS_REQUIRED(lock());

  void TryScheduleSequence(Sequence* sequence) EXCLUSIVE_LOCKS_REQUIRED(lock());

  // Returns a sorted list of runnable sequences.
  const std::vector<SchedulingState>& GetSortedRunnableSequences(
      PerThreadState& thread_state) EXCLUSIVE_LOCKS_REQUIRED(lock());

  // Returns true if there are *any* unblocked tasks in sequences assigned to
  // |task_runner|. This is used to decide if RunNextTask needs to be
  // rescheduled.
  bool HasAnyUnblockedTasksOnRunner(
      const base::SingleThreadTaskRunner* task_runner) const
      EXCLUSIVE_LOCKS_REQUIRED(lock());

  // Finds the sequence of the next task that can be run under |root_sequence|'s
  // dependency graph. This function will visit sequences that are tied to other
  // threads in case other threads depends on work on this thread; however, it
  // will not run any *leaves* that are tied to other threads. nullptr is
  // returned only if DrDC is enabled and when a sequence's only dependency is a
  // sequence that is tied to another thread.
  Sequence* FindNextTaskFromRoot(Sequence* root_sequence)
      EXCLUSIVE_LOCKS_REQUIRED(lock());

  // Calls |FindNextTaskFromRoot| on the ordered list of all sequences, and
  // returns the first runnable task. Returns nullptr if there is no work to do.
  Sequence* FindNextTask() EXCLUSIVE_LOCKS_REQUIRED(lock());

  // Executes the closure of the first task for |sequence_id|. Assumes that
  // the sequence has tasks, and that the first task is unblocked.
  void ExecuteSequence(SequenceId sequence_id) LOCKS_EXCLUDED(lock());

  // The scheduler's main loop. At each tick, it will call FindNextTask on the
  // sorted list of sequences to find the highest priority available sequence.
  // The scheduler then walks the sequence's dependency graph using DFS to find
  // any unblocked task. If there are multiple choices at any node, the
  // scheduler picks the dependency with the lowest SchedulingState, effectively
  // ordering the dependencies by priority and order_num.
  void RunNextTask() LOCKS_EXCLUDED(lock());

  base::flat_map<base::SingleThreadTaskRunner*, PerThreadState>
      per_thread_state_map_ GUARDED_BY(lock());

  // Accumulated time the thread was blocked during running task
  base::TimeDelta total_blocked_time_ GUARDED_BY(lock());
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_H_
