// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_INTERFERENCE_RECORDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_INTERFERENCE_RECORDER_H_

#include <atomic>
#include <map>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequence_manager/enqueue_order.h"
#include "base/task/sequence_manager/lazy_now.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {
namespace scheduler {

class MainThreadTaskQueue;

// Records the RendererScheduler.TimeRunningOtherAgentsWhileTaskReady histogram,
// which tracks how much time is spent running tasks from other agents between
// when an agent task becomes ready to run and when it starts running.
//
// Implementation details
// ----------------------
//
// The time running tasks from other agents while a task is ready is defined as:
//
//   ([Time running tasks for all agents between thread start and task start] -
//    [Time running tasks for all agents between thread start and task ready]) -
//   ([Time running tasks for same agent between thread start and task start] -
//    [Time running tasks for same agent between thread start and task ready]) -
//
// To get these values, we maintain the following state:
//
// A) For the task that is currently running:
//     A1) Start time
//     A2) Associated agent
// B) For each agent, an accumulator of time running tasks for that agent since
//    thread start.
// C) An accumulator of time running tasks for any agent since thread start.
// D) For each queued task for which we want to record the histogram:
//     D1) Time running tasks for all agents between thread start and task
//         queued.
//     D2) Time running tasks for each agent between thread start and task
//         queued.
//
// At any given time, we can compute:
//
// E) The time running tasks for a specific AGENT since thread start:
//      Read the AGENT's accumulator in B. Add [Current Time] - A1 if A2 is
//      equal to AGENT.
// F) The time running tasks for any agent since thread start:
//      Read C. Add [Current Time] - A1 if A2 is non-null.
//
// When a task becomes ready, we decide whether we want to record the histogram
// for it. If so, we use E and F to add an entry to D.
//
// When a task starts running, we check whether it is in D. If so, we use D1,
// D2, E and F to compute the value to record to the histogram. Then, we update
// A. When a task finishes running, we update B and C and we clear A. Note that
// even though we sample the tasks for which we record the histogram, we need to
// accumulate the time all tasks in B and C since it can contribute to the value
// recorded for a sampled task.
//
// Entering a nested loop is equivalent to finishing the current task. Exiting a
// nested loop is equivalent to resuming the task that was finished when the
// nested loop was entered (no histogram is recorded when resuming an existing
// task).
// TODO(crbug.com/1019856): Rename to AgentInterferenceRecorder.
class PLATFORM_EXPORT FrameInterferenceRecorder {
 public:
  // The histogram is recorded for 1 out of |sampling_rate| tasks.
  explicit FrameInterferenceRecorder(int sampling_rate = 1000);
  ~FrameInterferenceRecorder();

  // Invoked when a task becomes ready. For a non-delayed task, this is at post
  // time. For a delayed task, this is when the task's delay expires.
  //
  // |frame| is the FrameScheduler associated with the task (passed as void*
  // because it's only used a key and FrameScheduler isn't thread-safe - void
  // prevents undesired access).
  //
  // This is the only public method of this class that can be called from
  // another thread than the main thread.
  void OnTaskReady(const void* frame_scheduler,
                   base::sequence_manager::EnqueueOrder enqueue_order,
                   base::sequence_manager::LazyNow* lazy_now);

  // Invoked from the main thread when a task starts/finishes running, or when a
  // nested loop is exited/entered. When a nested loop is exited,
  // |enqueue_order| is EnqueueOrder::none().
  void OnTaskStarted(MainThreadTaskQueue* queue,
                     base::sequence_manager::EnqueueOrder enqueue_order,
                     base::TimeTicks start_time);
  void OnTaskCompleted(MainThreadTaskQueue* queue, base::TimeTicks end_time);

  // Invoked at the end of the destructor of a FrameScheduler, on the main
  // thread. Cleans up state associated with that FrameScheduler. Does not
  // dereference |frame_scheduler|.
  void OnFrameSchedulerDestroyed(const FrameScheduler* frame_scheduler);

 private:
  // Information about an agent.
  struct AgentData {
    AgentData() = default;

    // Time running tasks for agent since thread start.
    base::TimeDelta accumulated_running_time;

    // Number of frames associated with this agent.
    size_t frame_count = 0;
  };

  using AgentDataMap = std::map<base::UnguessableToken, AgentData>;

  // Information about a ready task for which the histogram will be recorded.
  struct ReadyTask {
    // Time running tasks for all agents when the task became ready (corresponds
    // to D1 above).
    base::TimeDelta time_for_all_agents_when_ready;

    // Time running tasks for each agent when the task became ready (corresponds
    // to D2 above).
    AgentDataMap agent_data_when_ready;

#if DCHECK_IS_ON()
    // The FrameScheduler associated with the task. Stored in a void* because
    // it's only used a key and FrameScheduler isn't thread-safe (so void
    // prevents undesired access).
    const void* frame_scheduler = nullptr;
#endif
  };

  // Updates |time_for_all_agents_| and
  // |agent_data_[current_task_agent_cluster_id_]| so that they reflect current
  // running time.
  void AccumulateCurrentTaskRunningTime(
      base::sequence_manager::LazyNow* lazy_now)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns the running time of the current agent task. Cannot be called if
  // currently running task isn't an agent task.
  base::TimeDelta GetCurrentAgentTaskRunningTime(
      base::sequence_manager::LazyNow* lazy_now) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Records the histogram for |ready_task|.
  void RecordHistogramForReadyTask(
      const ReadyTask& ready_task,
      const MainThreadTaskQueue* queue,
      const FrameScheduler* frame_scheduler,
      const base::UnguessableToken& agent_cluster_id,
      base::sequence_manager::EnqueueOrder enqueue_order)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns true if the next ready task should be sampled. Thread-safe.
  uint32_t ShouldSampleNextReadyTask();

  void DecrementNumFramesForAgent(
      const base::UnguessableToken& agent_cluster_id)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Virtual for testing.
  virtual void RecordHistogram(const MainThreadTaskQueue* queue,
                               base::TimeDelta sample);
  virtual const FrameScheduler* GetFrameSchedulerForQueue(
      const MainThreadTaskQueue* queue);
  virtual const base::UnguessableToken& GetAgentClusterIdForQueue(
      const MainThreadTaskQueue* queue);

  SEQUENCE_CHECKER(sequence_checker_);

  // Sampling rate. The histogram is recorded for 1/|sampling_rate_| tasks.
  const int sampling_rate_;

  // Current random value. Used to determine which tasks are sampled.
  std::atomic<uint32_t> random_value_;

  // Protects all members below. Low contention because most accesses are from
  // the main thread. Is only occasionally acquired from other threads when
  // OnTaskReady() decides to record the histogram for a task.
  base::Lock lock_;

  // Start time of the currently running task, or is_null() if no task is
  // running.
  base::TimeTicks current_task_start_time_ GUARDED_BY(lock_);

  // Agent cluster id of the currently running task, or Null if the task is not
  // agent-bound or if no task is running.
  base::UnguessableToken agent_cluster_id_for_current_task_ GUARDED_BY(lock_);

  // Time spent running tasks for all agents since thread start.
  base::TimeDelta time_for_all_agents_ GUARDED_BY(lock_);

  AgentDataMap agent_data_ GUARDED_BY(lock_);

  // Association between frames and agents. Frame is stored as a void* because
  // it's only used a key and FrameScheduler isn't thread-safe (so void prevents
  // undesired access). The mapping from frame to agent cluster id is maintained
  // to allow efficiently maintaining the frame count in AgentData. Protected by
  // |sequence_checker_|.
  WTF::HashMap<const void*, base::UnguessableToken> frame_to_agent_cluster_id_;

  // Information about ready tasks for which the histogram will be recorded. Key
  // is the task's sequence number.
  base::flat_map<base::sequence_manager::EnqueueOrder, ReadyTask> ready_tasks_
      GUARDED_BY(lock_);

  DISALLOW_COPY_AND_ASSIGN(FrameInterferenceRecorder);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_FRAME_INTERFERENCE_RECORDER_H_
