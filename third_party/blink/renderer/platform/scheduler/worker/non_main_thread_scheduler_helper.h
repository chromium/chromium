// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_NON_MAIN_THREAD_SCHEDULER_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_NON_MAIN_THREAD_SCHEDULER_HELPER_H_

#include "third_party/blink/renderer/platform/scheduler/common/scheduler_helper.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_task_queue.h"

namespace blink {
namespace scheduler {

class NonMainThreadSchedulerImpl;

// TODO(carlscab): This class is not really needed and should be removed
class PLATFORM_EXPORT NonMainThreadSchedulerHelper : public SchedulerHelper {
 public:
  // |sequence_manager| must remain valid until Shutdown() is called or the
  // object is destroyed. |main_thread_scheduler| must remain valid for the
  // entire lifetime of this object.
  NonMainThreadSchedulerHelper(
      base::sequence_manager::SequenceManager* manager,
      NonMainThreadSchedulerImpl* non_main_thread_scheduler,
      TaskType default_task_type);
  ~NonMainThreadSchedulerHelper() override;

  scoped_refptr<NonMainThreadTaskQueue> NewTaskQueue(
      const base::sequence_manager::TaskQueue::Spec& spec);

  scoped_refptr<NonMainThreadTaskQueue> DefaultNonMainThreadTaskQueue();
  scoped_refptr<NonMainThreadTaskQueue> ControlNonMainThreadTaskQueue();

  scoped_refptr<base::SingleThreadTaskRunner> DefaultNonMainThreadTaskRunner();
  scoped_refptr<base::SingleThreadTaskRunner> ControlNonMainThreadTaskRunner();

 protected:
  scoped_refptr<base::sequence_manager::TaskQueue> DefaultTaskQueue() override;
  scoped_refptr<base::sequence_manager::TaskQueue> ControlTaskQueue() override;
  void ShutdownAllQueues() override;

 private:
  NonMainThreadSchedulerImpl* non_main_thread_scheduler_;  // NOT OWNED
  const scoped_refptr<NonMainThreadTaskQueue> default_task_queue_;
  const scoped_refptr<NonMainThreadTaskQueue> control_task_queue_;

  DISALLOW_COPY_AND_ASSIGN(NonMainThreadSchedulerHelper);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_NON_MAIN_THREAD_SCHEDULER_HELPER_H_
