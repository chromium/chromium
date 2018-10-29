// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_WORKER_THREAD_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_WORKER_THREAD_H_

#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/atomic_flag.h"
#include "base/threading/thread.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace base {
class WaitableEvent;
}

namespace blink {
class ThreadScheduler;
}

namespace blink {
namespace scheduler {

class NonMainThreadSchedulerImpl;
class NonMainThreadTaskQueue;
class WorkerSchedulerProxy;

// Thread implementation for a thread created by Blink. Although the name says
// "worker", the thread represented by this class is used not only for Web
// Workers but for many other use cases, like for WebAudio, Web Database, etc.
//
// TODO(yutak): This could be a misnomer, as we already have WorkerThread in
// core/ (though this is under blink::scheduler namespace).
class PLATFORM_EXPORT WorkerThread
    : public Thread,
      public base::MessageLoopCurrent::DestructionObserver {
 public:
  explicit WorkerThread(const ThreadCreationParams& params);
  ~WorkerThread() override;

  // Thread implementation.
  void Init() override;
  ThreadScheduler* Scheduler() override;
  PlatformThreadId ThreadId() const override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() const override;

  // base::MessageLoopCurrent::DestructionObserver implementation.
  void WillDestroyCurrentMessageLoop() override;

  scheduler::NonMainThreadSchedulerImpl* GetNonMainThreadScheduler() {
    return non_main_thread_scheduler_.get();
  }

  scheduler::WorkerSchedulerProxy* worker_scheduler_proxy() const {
    return worker_scheduler_proxy_.get();
  }

 protected:
  virtual std::unique_ptr<NonMainThreadSchedulerImpl>
  CreateNonMainThreadScheduler();

  base::Thread* GetThread() const { return thread_.get(); }

  // protected instead of private for unit tests.
  scoped_refptr<base::SingleThreadTaskRunner> thread_task_runner_;

 private:
  void InitOnThread(base::WaitableEvent* completion);
  void ShutdownOnThread(base::WaitableEvent* completion);

  std::unique_ptr<base::Thread> thread_;
  const WebThreadType thread_type_;
  std::unique_ptr<scheduler::WorkerSchedulerProxy> worker_scheduler_proxy_;
  std::unique_ptr<scheduler::NonMainThreadSchedulerImpl>
      non_main_thread_scheduler_;
  scoped_refptr<NonMainThreadTaskQueue> task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::AtomicFlag was_shutdown_on_thread_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_WORKER_THREAD_H_
