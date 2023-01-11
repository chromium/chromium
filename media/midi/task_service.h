// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_TASK_SERVICE_H_
#define MEDIA_MIDI_TASK_SERVICE_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/midi/midi_export.h"

namespace midi {

// TaskService manages TaskRunners that can be used in midi and provides
// functionalities to ensure thread safety.
class MIDI_EXPORT TaskService final {
 public:
  using RunnerId = size_t;
  using InstanceId = int64_t;

  static constexpr RunnerId kDefaultRunnerId = 0;

  TaskService();

  TaskService(const TaskService&) = delete;
  TaskService& operator=(const TaskService&) = delete;

  ~TaskService();

  // Issues an InstanceId internally to post tasks via PostBoundTask() and
  // PostDelayedBoundTask() with the InstanceId. Once UnbindInstance() is
  // called, tasks posted via these methods with unbind InstanceId won't be
  // invoked any more.
  // Returns true if call is bound or unbound correctly. Otherwise returns
  // false, that happens when the BindInstance() is called twice without
  // unbinding the previous instance, or the UnbindInstance() is called without
  // any successful BindInstance() call.
  [[nodiscard]] bool BindInstance();
  [[nodiscard]] bool UnbindInstance();

  // Checks if the current thread belongs to the specified runner.
  bool IsOnTaskRunner(RunnerId runner_id);

  // Posts a task to run on a specified TaskRunner. |runner_id| should be a
  // positive number that represents a dedicated thread on that |task| will run.
  // |task| will run even without a bound instance.
  void PostStaticTask(RunnerId runner_id, base::OnceClosure task);

  // Posts a task to run on a specified TaskRunner, and ensures that the bound
  // instance should not quit UnbindInstance() while a bound task is running.
  // |runner_id| should be |kDefaultRunnerId| or a positive number. If
  // |kDefaultRunnerId| is specified, the task runs on the thread on which
  // BindInstance() was called.
  void PostBoundTask(RunnerId runner, base::OnceClosure task);
  void PostBoundDelayedTask(RunnerId runner_id,
                            base::OnceClosure task,
                            base::TimeDelta delay);

  void OverflowInstanceIdForTesting();

 private:
  static constexpr TaskService::InstanceId kInvalidInstanceId = -1;

  // Returns a SingleThreadTaskRunner reference. Each TaskRunner will be
  // constructed on demand.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(RunnerId runner_id);

  // Helps to run a posted bound task on TaskRunner safely.
  void RunTask(InstanceId instance_id,
               RunnerId runner_id,
               base::OnceClosure task);

  // Returns true if |instance_id| is equal to |bound_instance_id_|.
  bool IsInstanceIdStillBound(InstanceId instance_id);

  // Holds InstanceId for the next bound instance, accessed on the BindInstance
  // call thread without any protection.
  InstanceId next_instance_id_ = kInvalidInstanceId;

  // Keeps a TaskRunner for the thread that calls BindInstance() as a default
  // task runner to run posted tasks.
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_
      GUARDED_BY(lock_);

  // Holds threads to host SingleThreadTaskRunners.
  std::vector<std::unique_ptr<base::Thread>> threads_ GUARDED_BY(lock_);

  // Holds InstanceId for the current bound instance.
  InstanceId bound_instance_id_ GUARDED_BY(lock_) = kInvalidInstanceId;

  base::Lock lock_;

  // Signalled when the number of tasks in flight is 0 and ensures that
  // UnbindInstance() does not return until all tasks have completed.
  base::ConditionVariable no_tasks_in_flight_cv_
      GUARDED_BY(tasks_in_flight_lock_);

  // Number of tasks in flight.
  int tasks_in_flight_ GUARDED_BY(tasks_in_flight_lock_) = 0;

  base::Lock tasks_in_flight_lock_;

  // Verifies all UnbindInstance() calls occur on the same sequence as
  // BindInstance().
  SEQUENCE_CHECKER(instance_binding_sequence_checker_);
};

}  // namespace midi

#endif  // MEDIA_MIDI_TASK_SERVICE_H_
