// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THREAD_SCHEDULER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THREAD_SCHEDULER_IMPL_H_

#include "third_party/blink/renderer/platform/platform_export.h"

#include <random>

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/common/single_thread_idle_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace base {
class TickClock;

namespace sequence_manager {
class TimeDomain;
}
}  // namespace base

namespace v8 {
class Isolate;
}

namespace blink {
namespace scheduler {

// Scheduler-internal interface for the common methods between
// MainThreadSchedulerImpl and NonMainThreadSchedulerImpl which should
// not be exposed outside the scheduler.
class PLATFORM_EXPORT ThreadSchedulerImpl : public ThreadScheduler,
                                            public WebThreadScheduler {
 public:
  // This type is defined in both ThreadScheduler and WebThreadScheduler,
  // so the use of this type causes ambiguous lookup. Redefine this again
  // to hide the base classes' ones.
  using RendererPauseHandle = WebThreadScheduler::RendererPauseHandle;

  // Returns the idle task runner. Tasks posted to this runner may be reordered
  // relative to other task types and may be starved for an arbitrarily long
  // time if no idle time is available.
  virtual scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() = 0;

  virtual scoped_refptr<base::SingleThreadTaskRunner> ControlTaskRunner() = 0;

  virtual void RegisterTimeDomain(
      base::sequence_manager::TimeDomain* time_domain) = 0;
  virtual void UnregisterTimeDomain(
      base::sequence_manager::TimeDomain* time_domain) = 0;
  virtual base::sequence_manager::TimeDomain* GetActiveTimeDomain() = 0;

  virtual const base::TickClock* GetTickClock() = 0;

  void SetV8Isolate(v8::Isolate* isolate) override { isolate_ = isolate; }
  v8::Isolate* isolate() const { return isolate_; }

 protected:
  ThreadSchedulerImpl() {}
  ~ThreadSchedulerImpl() override = default;

  v8::Isolate* isolate_ = nullptr;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_THREAD_SCHEDULER_IMPL_H_
