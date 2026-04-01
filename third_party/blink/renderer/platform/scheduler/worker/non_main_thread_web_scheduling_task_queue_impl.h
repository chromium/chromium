// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_NON_MAIN_THREAD_WEB_SCHEDULING_TASK_QUEUE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_NON_MAIN_THREAD_WEB_SCHEDULING_TASK_QUEUE_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"

namespace blink::scheduler {
class NonMainThreadTaskQueue;
class WorkerSchedulerImpl;

class PLATFORM_EXPORT NonMainThreadWebSchedulingTaskQueueImpl
    : public WebSchedulingTaskQueue {
 public:
  NonMainThreadWebSchedulingTaskQueueImpl(
      base::WeakPtr<WorkerSchedulerImpl>,
      scoped_refptr<NonMainThreadTaskQueue>);
  ~NonMainThreadWebSchedulingTaskQueueImpl() override;

  void SetPriority(WebSchedulingPriority) override;

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override;

 private:
  base::WeakPtr<WorkerSchedulerImpl> scheduler_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // This object effectively owns `task_queue_`, but `scheduler_` also has a
  // reference to the queue so it can handle pausing for virtual time or
  // BFCache. That reference gets cleared during destruction.
  const scoped_refptr<NonMainThreadTaskQueue> task_queue_;
};

}  // namespace blink::scheduler

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_NON_MAIN_THREAD_WEB_SCHEDULING_TASK_QUEUE_IMPL_H_
