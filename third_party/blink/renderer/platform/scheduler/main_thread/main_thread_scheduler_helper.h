// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_SCHEDULER_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_SCHEDULER_HELPER_H_

#include "third_party/blink/renderer/platform/scheduler/common/scheduler_helper.h"

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"

namespace blink {
namespace scheduler {

class MainThreadSchedulerImpl;

// TODO(carlscab): This class is not really needed and should be removed
class PLATFORM_EXPORT MainThreadSchedulerHelper : public SchedulerHelper {
 public:
  // |sequence_manager| must remain valid until Shutdown() is called or the
  // object is destroyed. |main_thread_scheduler| must remain valid for the
  // entire lifetime of this object.
  MainThreadSchedulerHelper(
      base::sequence_manager::SequenceManager* sequence_manager,
      MainThreadSchedulerImpl* main_thread_scheduler);
  ~MainThreadSchedulerHelper() override;

  scoped_refptr<MainThreadTaskQueue> NewTaskQueue(
      const MainThreadTaskQueue::QueueCreationParams& params);

  scoped_refptr<MainThreadTaskQueue> DefaultMainThreadTaskQueue();
  scoped_refptr<MainThreadTaskQueue> ControlMainThreadTaskQueue();
  scoped_refptr<base::SingleThreadTaskRunner> DeprecatedDefaultTaskRunner();

 protected:
  scoped_refptr<base::sequence_manager::TaskQueue> DefaultTaskQueue() override;
  scoped_refptr<base::sequence_manager::TaskQueue> ControlTaskQueue() override;
  void ShutdownAllQueues() override;

 private:
  MainThreadSchedulerImpl* main_thread_scheduler_;  // NOT OWNED

  const scoped_refptr<MainThreadTaskQueue> default_task_queue_;
  const scoped_refptr<MainThreadTaskQueue> control_task_queue_;

  DISALLOW_COPY_AND_ASSIGN(MainThreadSchedulerHelper);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_SCHEDULER_HELPER_H_
