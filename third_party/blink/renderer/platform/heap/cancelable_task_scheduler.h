// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_CANCELABLE_TASK_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_CANCELABLE_TASK_SCHEDULER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace base {
class TaskRunner;
}

namespace blink {

// CancelableTaskScheduler allows for scheduling tasks that can be cancelled
// before they are invoked. User is responsible for synchronizing completion of
// tasks and destruction of CancelableTaskScheduler.
class PLATFORM_EXPORT CancelableTaskScheduler final {
  USING_FAST_MALLOC(CancelableTaskScheduler);

 public:
  using Task = WTF::CrossThreadOnceFunction<void()>;

  explicit CancelableTaskScheduler(scoped_refptr<base::TaskRunner>);
  ~CancelableTaskScheduler();

  // Schedules task to run on TaskRunner.
  void ScheduleTask(Task);
  // Cancels all not yet started tasks and waits for running ones to complete.
  // Returns number of cancelled (not executed) tasks.
  size_t CancelAndWait();

 private:
  class TaskData;
  template <class T>
  friend class CancelableTaskSchedulerTest;

  std::unique_ptr<TaskData> Register(Task);
  void UnregisterAndSignal(TaskData*);

  size_t RemoveCancelledTasks();

  size_t NumberOfTasksForTesting() const;

  WTF::HashSet<TaskData*> tasks_;
  mutable base::Lock lock_;
  base::ConditionVariable cond_var_;
  scoped_refptr<base::TaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_CANCELABLE_TASK_SCHEDULER_H_
