// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_interference_recorder.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

namespace {

// Park–Miller random number generator. Not secure, used only for sampling.
// https://en.wikipedia.org/wiki/Lehmer_random_number_generator
uint32_t GetNextRandomNumber(uint32_t previous_value) {
  constexpr uint32_t kModulo = (1U << 31) - 1;
  return 48271 * static_cast<int64_t>(previous_value) % kModulo;
}

}  // namespace

FrameInterferenceRecorder::FrameInterferenceRecorder(int sampling_rate)
    : sampling_rate_(sampling_rate),
      random_value_(static_cast<uint32_t>(base::RandUint64())) {}

FrameInterferenceRecorder::~FrameInterferenceRecorder() = default;

void FrameInterferenceRecorder::OnTaskReady(
    const void* frame_scheduler,
    base::sequence_manager::EnqueueOrder enqueue_order,
    base::sequence_manager::LazyNow* lazy_now) {
  if (!ShouldSampleNextReadyTask())
    return;

  if (!frame_scheduler)
    return;

  base::AutoLock auto_lock(lock_);

  DCHECK(!base::Contains(ready_tasks_, enqueue_order));
  ReadyTask& ready_task = ready_tasks_[enqueue_order];
  ready_task.time_for_all_agents_when_ready = time_for_all_agents_;
  ready_task.agent_data_when_ready = agent_data_;
#if DCHECK_IS_ON()
  ready_task.frame_scheduler = frame_scheduler;
#endif

  // If the currently running task is associated with an agent, adjust the
  // clock readings in |ready_task| to include its execution time.
  if (!agent_cluster_id_for_current_task_)
    return;

  base::TimeDelta running_time = GetCurrentAgentTaskRunningTime(lazy_now);
  // Clamp the value above zero to handle the case where the time in |lazy_now|
  // was captured after the current main thread task started running.
  running_time = std::max(base::TimeDelta(), running_time);

  ready_task.time_for_all_agents_when_ready += running_time;
  auto agent_data_it =
      ready_task.agent_data_when_ready.find(agent_cluster_id_for_current_task_);
  // The agent may have been destroyed before the task completes.
  if (agent_data_it != ready_task.agent_data_when_ready.end()) {
    agent_data_it->second.accumulated_running_time += running_time;
  }
}

void FrameInterferenceRecorder::OnTaskStarted(
    MainThreadTaskQueue* queue,
    base::sequence_manager::EnqueueOrder enqueue_order,
    base::TimeTicks start_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock auto_lock(lock_);
  DCHECK(current_task_start_time_.is_null());
  DCHECK(!agent_cluster_id_for_current_task_);

  const FrameScheduler* frame_scheduler =
      queue ? GetFrameSchedulerForQueue(queue) : nullptr;
  const base::UnguessableToken agent_cluster_id =
      queue ? GetAgentClusterIdForQueue(queue) : base::UnguessableToken();

  // Insert a default AgentData if there's no value for |agent_cluster_id|.
  if (agent_cluster_id)
    agent_data_.emplace(agent_cluster_id, AgentData{});

  auto ready_task_it = ready_tasks_.find(enqueue_order);
  if (ready_task_it != ready_tasks_.end()) {
    RecordHistogramForReadyTask(ready_task_it->second, queue, frame_scheduler,
                                agent_cluster_id, enqueue_order);
    ready_tasks_.erase(ready_task_it);
  }

  current_task_start_time_ = start_time;
  agent_cluster_id_for_current_task_ = agent_cluster_id;

  if (!frame_scheduler)
    return;

  // Maintain the mapping from frame to agent cluster id.
  auto frame_to_agent_it = frame_to_agent_cluster_id_.find(frame_scheduler);
  // If the frame is already tracked and has the same agent as before.
  if (frame_to_agent_it != frame_to_agent_cluster_id_.end() &&
      frame_to_agent_it->value == agent_cluster_id) {
    return;
  }
  // If the frame is already tracked, but has a new agent.
  if (frame_to_agent_it != frame_to_agent_cluster_id_.end()) {
    DecrementNumFramesForAgent(frame_to_agent_it->value);
    frame_to_agent_it->value = agent_cluster_id;
  } else {
    // If the frame was not tracked before.
    frame_to_agent_cluster_id_.insert(frame_scheduler, agent_cluster_id);
  }
  DCHECK_EQ(frame_to_agent_cluster_id_.find(frame_scheduler)->value,
            agent_cluster_id);
  // If the frame was not tracked before, or was tracked but has a new agent.
  if (agent_cluster_id)
    ++agent_data_.at(agent_cluster_id).frame_count;
}

void FrameInterferenceRecorder::OnTaskCompleted(MainThreadTaskQueue* queue,
                                                base::TimeTicks end_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock auto_lock(lock_);
  DCHECK(!end_time.is_null());

  base::sequence_manager::LazyNow lazy_now(end_time);
  AccumulateCurrentTaskRunningTime(&lazy_now);
  current_task_start_time_ = base::TimeTicks();
  agent_cluster_id_for_current_task_ = base::UnguessableToken();
}

void FrameInterferenceRecorder::OnFrameSchedulerDestroyed(
    const FrameScheduler* frame_scheduler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock auto_lock(lock_);

  auto frame_to_agent_it = frame_to_agent_cluster_id_.find(frame_scheduler);
  if (frame_to_agent_it == frame_to_agent_cluster_id_.end())
    return;
  DecrementNumFramesForAgent(frame_to_agent_it->value);
  frame_to_agent_cluster_id_.erase(frame_to_agent_it);
}

void FrameInterferenceRecorder::AccumulateCurrentTaskRunningTime(
    base::sequence_manager::LazyNow* lazy_now) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!current_task_start_time_.is_null());

  if (!agent_cluster_id_for_current_task_)
    return;

  // Update clocks.
  const base::TimeDelta running_time = GetCurrentAgentTaskRunningTime(lazy_now);
  DCHECK_GE(running_time, base::TimeDelta());

  time_for_all_agents_ += running_time;

  auto agent_data_it = agent_data_.find(agent_cluster_id_for_current_task_);
  // The agent may have been destroyed before the task completes.
  if (agent_data_it != agent_data_.end()) {
    agent_data_it->second.accumulated_running_time += running_time;
    DCHECK_GE(time_for_all_agents_,
              agent_data_it->second.accumulated_running_time);
  }
}

base::TimeDelta FrameInterferenceRecorder::GetCurrentAgentTaskRunningTime(
    base::sequence_manager::LazyNow* lazy_now) const {
  DCHECK(agent_cluster_id_for_current_task_);
  DCHECK(!current_task_start_time_.is_null());
  return lazy_now->Now() - current_task_start_time_;
}

uint32_t FrameInterferenceRecorder::ShouldSampleNextReadyTask() {
  uint32_t previous_random_value =
      random_value_.load(std::memory_order_relaxed);
  uint32_t next_random_value;
  do {
    next_random_value = GetNextRandomNumber(previous_random_value);
  } while (!random_value_.compare_exchange_weak(
      previous_random_value, next_random_value, std::memory_order_relaxed));
  return next_random_value % sampling_rate_ == 0;
}

void FrameInterferenceRecorder::RecordHistogramForReadyTask(
    const ReadyTask& ready_task,
    const MainThreadTaskQueue* queue,
    const FrameScheduler* frame_scheduler,
    const base::UnguessableToken& agent_cluster_id,
    base::sequence_manager::EnqueueOrder enqueue_order) {
  // Record the histogram if the task is associated with an agent and wasn't
  // blocked by a fence or by the TaskQueue being disabled.
  if (agent_cluster_id.is_empty() ||
      enqueue_order < queue->GetLastUnblockEnqueueOrder()) {
    return;
  }

#if DCHECK_IS_ON()
  DCHECK_EQ(frame_scheduler, ready_task.frame_scheduler);
#endif

  // |time_for_all_agents_since_ready| and |time_for_this_agent_since_ready| are
  // clamped above zero to mitigate this problem:
  //
  // X = initial |time_for_all_agents_|.
  //
  // Thread 1: Captures time T1.
  //           Invokes OnTaskStarted with T1.
  //           Captures time T2.
  // Thread 2: Captures time T3.
  //           Invokes OnTaskReady with T3. [*]
  //             |time_for_all_agents_when_ready| = X + T3 - T1
  //             (Ready task is from the same agent as the running task)
  // Thread 1: Invokes OnTaskCompleted with T2.
  //             |time_for_all_agents_| += T2 - T1
  //           Invokes OnTaskStarted for the next task.
  //             |time_for_all_agent_since_ready|
  //                 = |time_for_all_agents_| - |time_for_all_agents_when_ready|
  //                 = (X + T2 - T1) - (X + T3 - T1)
  //                 = T2 - T3
  //             Which is a negative value.
  //
  // |time_for_other_agents_since_ready| is clamped above zero for the case
  // where the ready and running tasks are not from the same agent at [*]. In
  // that case, |ready_task.time_for_all_agents_when_ready| was incremented with
  // the current task's running time, but not
  // |ready_task.time_for_this_agents_when_ready|, which can cause
  // |time_for_this_agent_since_ready| to be greater than
  // |time_for_all_agents_since_ready|.
  //
  // Note: These problem exists because OnTask*() methods use times captured
  // outside the scope of |lock_|.
  auto time_for_this_agent_when_ready_it =
      ready_task.agent_data_when_ready.find(agent_cluster_id);
  const base::TimeDelta time_for_this_agent_when_ready =
      time_for_this_agent_when_ready_it ==
              ready_task.agent_data_when_ready.end()
          ? base::TimeDelta()
          : time_for_this_agent_when_ready_it->second.accumulated_running_time;

  DCHECK_GE(ready_task.time_for_all_agents_when_ready,
            time_for_this_agent_when_ready);
  const base::TimeDelta time_for_all_agents_since_ready = std::max(
      base::TimeDelta(),
      time_for_all_agents_ - ready_task.time_for_all_agents_when_ready);

  auto agent_data_it = agent_data_.find(agent_cluster_id);
  DCHECK(agent_data_it != agent_data_.end());
  const base::TimeDelta time_for_agent =
      agent_data_it->second.accumulated_running_time;
  const base::TimeDelta time_for_this_agent_since_ready = std::max(
      base::TimeDelta(), time_for_agent - time_for_this_agent_when_ready);

  const base::TimeDelta time_for_other_agents_since_ready =
      std::max(base::TimeDelta(), time_for_all_agents_since_ready -
                                      time_for_this_agent_since_ready);
  RecordHistogram(queue, time_for_other_agents_since_ready);
}

void FrameInterferenceRecorder::DecrementNumFramesForAgent(
    const base::UnguessableToken& agent_cluster_id) {
  if (!agent_cluster_id)
    return;
  auto agent_data_it = agent_data_.find(agent_cluster_id);
  DCHECK(agent_data_it != agent_data_.end());
  DCHECK_GT(agent_data_it->second.frame_count, 0U);
  // If |frame_count| reaches 0, the data for this agent is cleared as it's
  // no longer useful.
  if (--agent_data_it->second.frame_count == 0)
    agent_data_.erase(agent_data_it);
}

void FrameInterferenceRecorder::RecordHistogram(
    const MainThreadTaskQueue* queue,
    base::TimeDelta sample) {
  // Histogram should only be recorded for queue types that can be associated
  // with agents.
  DCHECK(GetAgentClusterIdForQueue(queue));
  const MainThreadTaskQueue::QueueType queue_type = queue->queue_type();
  DCHECK(MainThreadTaskQueue::IsPerFrameTaskQueue(queue_type));

  const std::string histogram_name =
      std::string("RendererScheduler.TimeRunningOtherAgentsWhileTaskReady.") +
      (GetFrameSchedulerForQueue(queue)->IsFrameVisible() ? "Visible"
                                                          : "Hidden");
  base::UmaHistogramCustomMicrosecondsTimes(
      histogram_name, sample, base::TimeDelta::FromMicroseconds(1),
      base::TimeDelta::FromSeconds(1), 100);
  const std::string histogram_name_with_queue_type =
      histogram_name + "." + MainThreadTaskQueue::NameForQueueType(queue_type);
  base::UmaHistogramCustomMicrosecondsTimes(
      histogram_name_with_queue_type, sample,
      base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromSeconds(1),
      100);
}

const FrameScheduler* FrameInterferenceRecorder::GetFrameSchedulerForQueue(
    const MainThreadTaskQueue* queue) {
  return queue->GetFrameScheduler();
}

const base::UnguessableToken&
FrameInterferenceRecorder::GetAgentClusterIdForQueue(
    const MainThreadTaskQueue* queue) {
  const FrameSchedulerImpl* frame_scheduler = queue->GetFrameScheduler();
  return frame_scheduler ? frame_scheduler->GetAgentClusterId()
                         : base::UnguessableToken::Null();
}

}  // namespace scheduler
}  // namespace blink
