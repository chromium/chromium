// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_NON_MAIN_THREAD_SCHEDULER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_NON_MAIN_THREAD_SCHEDULER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/common/single_thread_idle_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/common/thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_type.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_helper.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_task_queue.h"

namespace blink {
namespace scheduler {

class WorkerSchedulerProxy;

class PLATFORM_EXPORT NonMainThreadSchedulerImpl : public ThreadSchedulerImpl {
 public:
  ~NonMainThreadSchedulerImpl() override;

  // |sequence_manager| and |proxy| must remain valid for the entire lifetime of
  // this object.
  static std::unique_ptr<NonMainThreadSchedulerImpl> Create(
      ThreadType thread_type,
      base::sequence_manager::SequenceManager* sequence_manager,
      WorkerSchedulerProxy* proxy);

  // Blink should use NonMainThreadSchedulerImpl::DefaultTaskQueue instead of
  // WebThreadScheduler::DefaultTaskRunner.
  virtual scoped_refptr<NonMainThreadTaskQueue> DefaultTaskQueue() = 0;

  // Must be called before the scheduler can be used. Does any post construction
  // initialization needed such as initializing idle period detection.
  void Init();

  virtual void OnTaskCompleted(
      NonMainThreadTaskQueue* worker_task_queue,
      const base::sequence_manager::Task& task,
      base::sequence_manager::TaskQueue::TaskTiming* task_timing,
      base::sequence_manager::LazyNow* lazy_now) = 0;

  // ThreadSchedulerImpl:
  scoped_refptr<base::SingleThreadTaskRunner> ControlTaskRunner() override;
  void RegisterTimeDomain(
      base::sequence_manager::TimeDomain* time_domain) override;
  void UnregisterTimeDomain(
      base::sequence_manager::TimeDomain* time_domain) override;
  base::sequence_manager::TimeDomain* GetActiveTimeDomain() override;
  const base::TickClock* GetTickClock() override;

  // ThreadScheduler implementation.
  // TODO(yutak): Some functions are only meaningful in main thread. Move them
  // to MainThreadScheduler.
  void PostIdleTask(const base::Location& location,
                    Thread::IdleTask task) override;
  void PostNonNestableIdleTask(const base::Location& location,
                               Thread::IdleTask task) override;
  void PostDelayedIdleTask(const base::Location& location,
                           base::TimeDelta delay,
                           Thread::IdleTask task) override;

  std::unique_ptr<PageScheduler> CreatePageScheduler(
      PageScheduler::Delegate*) override;
  std::unique_ptr<RendererPauseHandle> PauseScheduler() override
      WARN_UNUSED_RESULT;

  // Returns base::TimeTicks::Now() by default.
  base::TimeTicks MonotonicallyIncreasingVirtualTime() override;

  NonMainThreadSchedulerImpl* AsNonMainThreadScheduler() override {
    return this;
  }

  // The following virtual methods are defined in *both* WebThreadScheduler
  // and ThreadScheduler, with identical interfaces and semantics. They are
  // overriden in a subclass, effectively implementing the virtual methods
  // in both classes at the same time. This is allowed in C++, as long as
  // there is only one final overrider (i.e. definitions in base classes are
  // not used in instantiated objects, since otherwise they may have multiple
  // definitions of the virtual function in question).
  //
  // virtual void Shutdown();

  scoped_refptr<NonMainThreadTaskQueue> CreateTaskQueue(const char* name);

  scoped_refptr<base::SingleThreadTaskRunner> DeprecatedDefaultTaskRunner()
      override;

 protected:
  static void RunIdleTask(Thread::IdleTask task, base::TimeTicks deadline);

  // |sequence_manager| must remain valid for the entire lifetime of
  // this object.
  explicit NonMainThreadSchedulerImpl(
      base::sequence_manager::SequenceManager* sequence_manager,
      TaskType default_task_type);

  friend class WorkerScheduler;

  // Called during Init() for delayed initialization for subclasses.
  virtual void InitImpl() = 0;

  NonMainThreadSchedulerHelper* helper() { return &helper_; }

 private:
  NonMainThreadSchedulerHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(NonMainThreadSchedulerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_WORKER_NON_MAIN_THREAD_SCHEDULER_IMPL_H_
