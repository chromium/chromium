// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_WORKER_THREAD_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_WORKER_THREAD_H_

#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/threading/simple_thread.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/renderer/platform/heap/gc_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
class BlinkGCMemoryDumpProvider;
class ThreadScheduler;
}

namespace blink {
namespace scheduler {

class NonMainThreadSchedulerImpl;
class WorkerSchedulerProxy;

// Thread implementation for a thread created by Blink. Although the name says
// "worker", the thread represented by this class is used not only for Web
// Workers but for many other use cases, like for WebAudio, Web Database, etc.
//
// TODO(yutak): This could be a misnomer, as we already have WorkerThread in
// core/ (though this is under blink::scheduler namespace).
class PLATFORM_EXPORT WorkerThread : public Thread {
 public:
  explicit WorkerThread(const ThreadCreationParams& params);
  ~WorkerThread() override;

  // Thread implementation.
  void Init() override;
  ThreadScheduler* Scheduler() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() const override;

  scheduler::NonMainThreadSchedulerImpl* GetNonMainThreadScheduler() {
    return thread_->GetNonMainThreadScheduler();
  }

  scheduler::WorkerSchedulerProxy* worker_scheduler_proxy() const {
    return worker_scheduler_proxy_.get();
  }

  // This should be eventually removed. It's needed for a very specific case
  // when we cannot wait until the underlying SimpleThreadImpl finishes, see
  // WorkerBackingThread::ShutdownOnBackingThread().
  void ShutdownOnThread() override;

 protected:
  virtual std::unique_ptr<NonMainThreadSchedulerImpl>
  CreateNonMainThreadScheduler(
      base::sequence_manager::SequenceManager* sequence_manager);

 private:
  class GCSupport;
  class SimpleThreadImpl final : public base::SimpleThread {
   public:
    using NonMainThreadSchedulerFactory = base::OnceCallback<
        std::unique_ptr<scheduler::NonMainThreadSchedulerImpl>(
            base::sequence_manager::SequenceManager*)>;

    explicit SimpleThreadImpl(const WTF::String& name_prefix,
                              const base::SimpleThread::Options& options,
                              bool supports_gc,
                              WorkerThread* worker_thread);

    // Creates the thread's scheduler. Must be invoked before starting the
    // thread or accessing the default TaskRunner.
    void CreateScheduler();

    // Attention: Can only be called after CreateScheduler().
    scoped_refptr<base::SingleThreadTaskRunner> GetDefaultTaskRunner() const {
      DCHECK(default_task_runner_);
      return default_task_runner_;
    }

    // Attention: Can only be called from the worker thread.
    scheduler::NonMainThreadSchedulerImpl* GetNonMainThreadScheduler() {
      DCHECK(GetDefaultTaskRunner()->RunsTasksInCurrentSequence());
      return non_main_thread_scheduler_.get();
    }

    // SimpleThreadImpl automatically calls this after exiting the Run() but
    // there are some use cases in which clients must call it before. See
    // WorkerThread::ShutdownOnThread().
    void ShutdownOnThread();

    // Makes sure that Run will eventually finish and thus the thread can be
    // joined.
    // Can be called from any thread.
    // Attention: Can only be called once.
    void Quit();

   private:
    void Run() override;

    // Internal queue not exposed externally nor to the scheduler used for
    // internal operations such as posting the task that will stop the run
    // loop.
    scoped_refptr<base::SingleThreadTaskRunner> internal_task_runner_;

    WorkerThread* thread_;

    // The following variables are "owned" by the worker thread
    std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager_;
    scoped_refptr<base::sequence_manager::TaskQueue> internal_task_queue_;
    std::unique_ptr<scheduler::NonMainThreadSchedulerImpl>
        non_main_thread_scheduler_;
    scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
    base::RunLoop* run_loop_;
    bool supports_gc_;
    std::unique_ptr<GCSupport> gc_support_;
  };

  class GCSupport final {
   public:
    explicit GCSupport(WorkerThread* thread);
    ~GCSupport();

   private:
    std::unique_ptr<GCTaskRunner> gc_task_runner_;
    std::unique_ptr<BlinkGCMemoryDumpProvider> blink_gc_memory_dump_provider_;
  };

  std::unique_ptr<SimpleThreadImpl> thread_;
  const ThreadType thread_type_;
  std::unique_ptr<scheduler::WorkerSchedulerProxy> worker_scheduler_proxy_;
  bool supports_gc_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_WORKER_THREAD_H_
