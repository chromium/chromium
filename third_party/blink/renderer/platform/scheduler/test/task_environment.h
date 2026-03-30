// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_TASK_ENVIRONMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_TASK_ENVIRONMENT_H_

#include <memory>
#include <optional>

#include "base/test/task_environment.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"

namespace blink {
namespace scheduler {
class MainThreadSchedulerImpl;
}  // namespace scheduler

namespace test {

// TaskEnvironmentWithMainThreadScheduler is a specialized class to instantiate
// a TaskEnvironment with MainThreadSchedulerImpl. This is useful in
// platform tests, but prefer using blink::test::TaskEnvironment otherwise,
// which instantiates a full Blink Main Thread.
class TaskEnvironmentWithMainThreadScheduler
    : public base::test::TaskEnvironment {
 public:
  using ValidTraits = base::test::TaskEnvironment::ValidTraits;

  template <typename... Traits>
    requires base::trait_helpers::AreValidTraits<ValidTraits, Traits...>
  explicit TaskEnvironmentWithMainThreadScheduler(Traits... traits)
      : TaskEnvironmentWithMainThreadScheduler(
            CreateTaskEnvironmentWithPriorities(
                blink::scheduler::CreatePrioritySettings(),
                SubclassCreatesDefaultTaskRunner{},
                traits...)) {}

  ~TaskEnvironmentWithMainThreadScheduler() override;

  scheduler::MainThreadSchedulerImpl* main_thread_scheduler() const {
    return scheduler_.get();
  }

 private:
  explicit TaskEnvironmentWithMainThreadScheduler(
      base::test::TaskEnvironment&& scoped_task_environment);

  std::unique_ptr<scheduler::MainThreadSchedulerImpl> scheduler_;
};

}  // namespace test
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_TASK_ENVIRONMENT_H_
