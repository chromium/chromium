// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_H_

#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {
class ThreadScheduler;
}

namespace blink {
namespace scheduler {
class MainThreadSchedulerImpl;

class PLATFORM_EXPORT MainThread : public Thread {
 public:
  explicit MainThread(MainThreadSchedulerImpl* scheduler);
  ~MainThread() override;

  // Thread implementation.
  ThreadScheduler* Scheduler() override;
  PlatformThreadId ThreadId() const override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() const override;

  void AddTaskTimeObserver(base::sequence_manager::TaskTimeObserver*) override;
  void RemoveTaskTimeObserver(
      base::sequence_manager::TaskTimeObserver*) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  MainThreadSchedulerImpl* scheduler_;  // Not owned.
  PlatformThreadId thread_id_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_MAIN_THREAD_H_
