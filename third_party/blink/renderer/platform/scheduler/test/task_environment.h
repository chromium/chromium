// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_TASK_ENVIRONMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_TASK_ENVIRONMENT_H_

#include <memory>
#include <optional>

#include "base/test/task_environment.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {
namespace scheduler {
class MainThreadSchedulerImpl;
class NonMainThreadSchedulerHelper;
}  // namespace scheduler

namespace test {

// These classes instantiate specific scheduling environments not provided by
// base::test::TaskEnvironment. This is intended to be used in blink platform
// tests, where a full Blink Main Thread (which includes the blink main thread
// and v8 isolate) isn't supported. Prefer using blink::test::TaskEnvironment in
// blink otherwise.

// A base::test::TaskEnvironment configured with blink-specific task priority
// settings. Use this for low-level scheduler tests that only need these
// priorities.
class TaskEnvironmentWithPrioritySettings : public base::test::TaskEnvironment {
 public:
  using ValidTraits = base::test::TaskEnvironment::ValidTraits;

  template <typename... Traits>
    requires base::trait_helpers::AreValidTraits<ValidTraits, Traits...>
  explicit TaskEnvironmentWithPrioritySettings(Traits... traits)
      : base::test::TaskEnvironment(CreateTaskEnvironmentWithPriorities(
            blink::scheduler::CreatePrioritySettings(),
            SubclassCreatesDefaultTaskRunner{},
            traits...)) {
    task_queue_ = sequence_manager()->CreateTaskQueue(
        base::sequence_manager::TaskQueue::Spec(
            base::sequence_manager::QueueName::TASK_ENVIRONMENT_DEFAULT_TQ));
    DeferredInitFromSubclass(task_queue_.get());
  }

  base::sequence_manager::TaskQueue::Handle CreateTaskQueue(
      const base::sequence_manager::TaskQueue::Spec& spec) {
    return sequence_manager()->CreateTaskQueue(spec);
  }

 private:
  base::sequence_manager::TaskQueue::Handle task_queue_;
};

// Instantiate a MainThreadSchedulerImpl or test subclass. Use this for testing
// the main thread scheduler itself or platform-level components that interact
// with it, when the full blink environment (isolate, etc.) is not required.
template <class MainThreadScheduler>
class TaskEnvironmentWithMainThreadSchedulerBase
    : public base::test::TaskEnvironment {
 public:
  using ValidTraits = base::test::TaskEnvironment::ValidTraits;

  template <typename... Traits>
    requires base::trait_helpers::AreValidTraits<ValidTraits, Traits...>
  explicit TaskEnvironmentWithMainThreadSchedulerBase(Traits... traits)
      : TaskEnvironmentWithMainThreadSchedulerBase(
            CreateTaskEnvironmentWithPriorities(
                blink::scheduler::CreatePrioritySettings(),
                SubclassCreatesDefaultTaskRunner{},
                traits...)) {}

  ~TaskEnvironmentWithMainThreadSchedulerBase() override {
    if (scheduler_) {
      scheduler_->Shutdown();
    }
  }

  MainThreadScheduler* GetMainThreadScheduler() const {
    return scheduler_.get();
  }

 private:
  explicit TaskEnvironmentWithMainThreadSchedulerBase(
      base::test::TaskEnvironment&& scoped_task_environment)
      : base::test::TaskEnvironment(std::move(scoped_task_environment)) {
    CHECK(IsMainThread());
    scheduler_ = std::make_unique<MainThreadScheduler>(sequence_manager());
    DeferredInitFromSubclass(scheduler_->DefaultTaskQueue()->GetTaskQueue());
  }

  std::unique_ptr<MainThreadScheduler> scheduler_;
};

using TaskEnvironmentWithMainThreadScheduler =
    TaskEnvironmentWithMainThreadSchedulerBase<
        scheduler::MainThreadSchedulerImpl>;

// Instantiate a NonMainThreadSchedulerHelper.
class TaskEnvironmentWithNonMainThreadSchedulerHelper
    : public base::test::TaskEnvironment {
 public:
  using ValidTraits = base::test::TaskEnvironment::ValidTraits;

  template <typename... Traits>
    requires base::trait_helpers::AreValidTraits<ValidTraits, Traits...>
  explicit TaskEnvironmentWithNonMainThreadSchedulerHelper(Traits... traits)
      : TaskEnvironmentWithNonMainThreadSchedulerHelper(
            CreateTaskEnvironmentWithPriorities(
                blink::scheduler::CreatePrioritySettings(),
                SubclassCreatesDefaultTaskRunner{},
                traits...)) {}

  ~TaskEnvironmentWithNonMainThreadSchedulerHelper() override;

  scheduler::NonMainThreadSchedulerHelper* GetNonMainThreadSchedulerHelper() {
    return scheduler_helper_.get();
  }

 private:
  explicit TaskEnvironmentWithNonMainThreadSchedulerHelper(
      base::test::TaskEnvironment&& scoped_task_environment);

  std::unique_ptr<scheduler::NonMainThreadSchedulerHelper> scheduler_helper_;
};

}  // namespace test
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_TASK_ENVIRONMENT_H_
