// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_FUZZER_THREAD_POOL_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_FUZZER_THREAD_POOL_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/sequence_manager/time_domain.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/proto/sequence_manager_test_description.pb.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace base {
namespace sequence_manager {

class SequenceManagerFuzzerProcessor;
class ThreadManager;

// Used by the SequenceManagerFuzzerProcessor to manage threads and synchronize
// their clocks.
class PLATFORM_EXPORT ThreadPoolManager {
  USING_FAST_MALLOC(ThreadPoolManager);

 public:
  explicit ThreadPoolManager(SequenceManagerFuzzerProcessor* processor,
                             base::TimeTicks initial_time);
  ~ThreadPoolManager();

  // |initial_time| is the time in which the thread is created.
  void CreateThread(
      const google::protobuf::RepeatedPtrField<
          SequenceManagerTestDescription::Action>& initial_thread_actions);

  base::TimeTicks NowTicks() const;
  bool MaybeFastForwardToWakeUp(std::optional<WakeUp> next_wake_up);

  // Advances the mock tick clock of all the threads synchronously, until it
  // reaches `time`.
  void AdvanceClockSynchronouslyToTime(base::TimeTicks time);

  // Used by a thread to notify the thread manager that it is done executing the
  // thread actions passed to ThreadPoolManager::CreateThread.
  void ThreadDone();

  void StartInitialThreads();

  // Used by the processor to wait for all of the threads to finish executing
  // the actions passed by ThreadPoolManager::CreateThread. Note that
  // the threads are not terminated until |this| gets destructed.
  void WaitForAllThreads();

  // (Thread Safe)
  blink::Vector<ThreadManager*> GetAllThreadManagers();

  // (Thread Safe) Used to return the thread manager of the |thread_id|'s entry
  // in |threads_| (modulo the number of entries).
  // Note: not all threads created with CreateThread() might have had time to
  // register their thread manager by the time this method is called as the
  // ThreadManager creation happens on the new thread. This method will return
  // nullptr if no ThreadManager was registered yet.
  ThreadManager* GetThreadManagerFor(uint64_t thread_id);

  SequenceManagerFuzzerProcessor* processor() const;

 private:
  void StartThread(
      const google::protobuf::RepeatedPtrField<
          SequenceManagerTestDescription::Action>& initial_thread_actions,
      ThreadManager* thread_manager);

  // Helper function used by AdvanceClockSynchronouslyToTime and
  // AdvanceClockSynchronouslyByPendingTaskDelay to notify the manager when all
  // threads are ready to compute their next desired time.
  void ThreadReadyToComputeTime();

  // Helper function used by AdvanceClockSynchronouslyToTime and
  // MaybeFastForwardToWakeUp to advance the thread's clock to |next_time_|.
  // Note that this function waits until all threads have voted on the value of
  // |next_time_|.
  void AdvanceClockSynchronouslyImpl() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Owner of this class.
  const raw_ptr<SequenceManagerFuzzerProcessor> processor_;

  // Used to protect all the members below.
  mutable Lock lock_;

  // Used to synchronize virtual time across all threads.
  base::TimeTicks next_time_ GUARDED_BY(lock_);

  base::TimeTicks now_ GUARDED_BY(lock_);

  // Condition that all threads are done and the program is ready to
  // terminate.
  ConditionVariable ready_to_terminate_;

  // Condition that threads can start running. This is needed to make sure all
  // of the initial (program entry points) threads were created before any
  // thread starts running.
  ConditionVariable ready_to_execute_threads_;

  // Condition that |next_time_| is computed and that all threads can advance
  // their clock to |next_time_|.
  ConditionVariable ready_to_advance_time_;

  // Number of threads ready to advance the clock and run the next round.
  uint64_t threads_waiting_to_advance_time_ GUARDED_BY(lock_){0};

  // Number of threads done executing their sequence of actions.
  uint64_t threads_ready_to_terminate_ GUARDED_BY(lock_){0};

  // Step used as barrier generation, incremented each time
  // AdvanceClockSynchronouslyImpl() is reached by all threads.
  uint64_t barrier_step_ GUARDED_BY(lock_){0};

  // Used to notify the condition |ready_to_start_threads_|.
  bool initial_threads_created_ GUARDED_BY(lock_);

  // Threads that are being managed/synchronized. For unit testing purposes,
  // make sure not to create threads at the same time (if the ordering matters)
  // since in this case the order will not be deterministic.
  blink::Vector<std::unique_ptr<SimpleThread>> threads_ GUARDED_BY(lock_);

  // ThreadManager instances associated to the managed threads. Values are not
  // stored in any particular order and there might not exist a manager for all
  // managed threads at any point in time (SimpleThread instances are created
  // before their corresponding ThreadManager, as this must happen on the actual
  // thread).
  blink::Vector<ThreadManager*> thread_managers_ GUARDED_BY(lock_);
};

}  // namespace sequence_manager
}  // namespace base

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_FUZZER_THREAD_POOL_MANAGER_H_
