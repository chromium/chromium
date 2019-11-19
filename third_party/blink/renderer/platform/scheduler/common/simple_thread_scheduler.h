// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_SIMPLE_THREAD_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_SIMPLE_THREAD_SCHEDULER_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {
namespace scheduler {

// ThreadScheduler implementation that should be used when you don't have a
// dedicated ThreadScheduler instance. This is essentially a scheduler doesn't
// really schedule anything -- it just runs tasks as they come.
//
// This scheduler does not implement several features (see comments at each
// function). If you invoke a Blink functionality that relies on an
// unimplemented scheduler function, you might observe crashes or misbehavior.
// Apparently we don't rely on those missing features at least for things that
// use this scheduler.

class SimpleThreadScheduler : public ThreadScheduler {
 public:
  SimpleThreadScheduler();
  ~SimpleThreadScheduler() override;

  // Do nothing.
  void Shutdown() override;

  // Always return false.
  bool ShouldYieldForHighPriorityWork() override;

  // Always return false.
  bool CanExceedIdleDeadlineIfRequired() const override;

  // Those tasks are simply ignored (we assume there's no idle period).
  void PostIdleTask(const base::Location&, Thread::IdleTask) override;
  void PostDelayedIdleTask(const base::Location&,
                           base::TimeDelta delay,
                           Thread::IdleTask) override;
  void PostNonNestableIdleTask(const base::Location&,
                               Thread::IdleTask) override;

  // Do nothing (the observer won't get notified).
  void AddRAILModeObserver(RAILModeObserver*) override;

  // Do nothing.
  void RemoveRAILModeObserver(RAILModeObserver const*) override;

  // Return the thread task runner (there's no separate task runner for them).
  scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> IPCTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> DeprecatedDefaultTaskRunner()
      override;

  // Unsupported. Return nullptr, and it may cause a crash.
  std::unique_ptr<PageScheduler> CreatePageScheduler(
      PageScheduler::Delegate*) override;

  // Unsupported. Return nullptr, and it may cause a crash.
  std::unique_ptr<RendererPauseHandle> PauseScheduler() override;

  // Return the current time.
  base::TimeTicks MonotonicallyIncreasingVirtualTime() override;

  // Unsupported. The observer won't get called. May break some functionalities
  // that rely on the task observer.
  void AddTaskObserver(base::TaskObserver*) override;
  void RemoveTaskObserver(base::TaskObserver*) override;

  // Return nullptr.
  NonMainThreadSchedulerImpl* AsNonMainThreadScheduler() override;

  void SetV8Isolate(v8::Isolate* isolate) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleThreadScheduler);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_SIMPLE_THREAD_SCHEDULER_H_
