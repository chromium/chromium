// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_SCHEDULER_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_SCHEDULER_HELPER_H_

#include <stddef.h>
#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/time/tick_clock.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {
namespace scheduler {

// Common scheduler functionality for default tasks.
class PLATFORM_EXPORT SchedulerHelper
    : public base::sequence_manager::SequenceManager::Observer {
 public:
  explicit SchedulerHelper(
      std::unique_ptr<base::sequence_manager::SequenceManager>
          sequence_manager);
  ~SchedulerHelper() override;

  // SequenceManager::Observer implementation:
  void OnBeginNestedRunLoop() override;
  void OnExitNestedRunLoop() override;

  const base::TickClock* GetClock() const;
  base::TimeTicks NowTicks() const;
  void SetTimerSlack(base::TimerSlack timer_slack);

  // Returns the default task queue.
  virtual scoped_refptr<base::sequence_manager::TaskQueue>
  DefaultTaskQueue() = 0;

  // Returns the control task queue.  Tasks posted to this queue are executed
  // with the highest priority. Care must be taken to avoid starvation of other
  // task queues.
  virtual scoped_refptr<base::sequence_manager::TaskQueue>
  ControlTaskQueue() = 0;

  // Returns task runner for the default queue.
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner();

  // Adds or removes a task observer from the scheduler. The observer will be
  // notified before and after every executed task. These functions can only be
  // called on the thread this class was created on.
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer);
  void RemoveTaskObserver(base::MessageLoop::TaskObserver* task_observer);

  void AddTaskTimeObserver(
      base::sequence_manager::TaskTimeObserver* task_time_observer);
  void RemoveTaskTimeObserver(
      base::sequence_manager::TaskTimeObserver* task_time_observer);

  // Shuts down the scheduler by dropping any remaining pending work in the work
  // queues. After this call any work posted to the task queue will be
  // silently dropped.
  void Shutdown();

  // Returns true if Shutdown() has been called. Otherwise returns false.
  bool IsShutdown() const { return !sequence_manager_.get(); }

  inline void CheckOnValidThread() const {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  class PLATFORM_EXPORT Observer {
   public:
    virtual ~Observer() = default;

    // Called when scheduler executes task with nested run loop.
    virtual void OnBeginNestedRunLoop() = 0;

    // Called when the scheduler spots we've exited a nested run loop.
    virtual void OnExitNestedRunLoop() = 0;
  };

  // Called once to set the Observer. This function is called on the main
  // thread. If |observer| is null, then no callbacks will occur.
  // Note |observer| is expected to outlive the SchedulerHelper.
  void SetObserver(Observer* observer);

  // Remove all canceled delayed tasks.
  void SweepCanceledDelayedTasks();

  // Accessor methods.
  base::sequence_manager::TimeDomain* real_time_domain() const;
  void RegisterTimeDomain(base::sequence_manager::TimeDomain* time_domain);
  void UnregisterTimeDomain(base::sequence_manager::TimeDomain* time_domain);
  bool GetAndClearSystemIsQuiescentBit();
  double GetSamplingRateForRecordingCPUTime() const;
  bool HasCPUTimingForEachTask() const;

  // Test helpers.
  void SetWorkBatchSizeForTesting(int work_batch_size);

 protected:
  void InitDefaultQueues(
      scoped_refptr<base::sequence_manager::TaskQueue> default_task_queue,
      scoped_refptr<base::sequence_manager::TaskQueue> control_task_queue,
      TaskType default_task_type);

  base::ThreadChecker thread_checker_;
  std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager_;

 private:
  friend class SchedulerHelperTest;

  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;

  Observer* observer_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(SchedulerHelper);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_SCHEDULER_HELPER_H_
