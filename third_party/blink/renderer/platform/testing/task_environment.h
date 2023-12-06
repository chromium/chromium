// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_TASK_ENVIRONMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_TASK_ENVIRONMENT_H_

#include "base/test/task_environment.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/testing/main_thread_isolate.h"
#include "third_party/blink/renderer/platform/testing/scoped_main_thread_overrider.h"

namespace blink::test {
namespace internal {

class TaskEnvironmentImpl : public base::test::TaskEnvironment {
 public:
  // Instantiates a full featured blink::MainThreadScheduler as opposed to a
  // simple Thread scheduler.
  struct RealMainThreadScheduler {};

  struct ValidTraits {
    explicit ValidTraits(base::test::TaskEnvironment::ValidTraits);
    explicit ValidTraits(RealMainThreadScheduler);
  };

  template <typename... Traits>
    requires base::trait_helpers::AreValidTraits<ValidTraits, Traits...>
  explicit TaskEnvironmentImpl(Traits... traits)
      : TaskEnvironmentImpl(
            CreateTaskEnvironmentWithPriorities(
                blink::scheduler::CreatePrioritySettings(),
                std::conditional_t<
                    base::trait_helpers::HasTrait<RealMainThreadScheduler,
                                                  Traits...>::value,
                    SubclassCreatesDefaultTaskRunner,
                    base::trait_helpers::EmptyTrait>{},
                base::trait_helpers::
                    Exclude<MainThreadType, RealMainThreadScheduler>::Filter(
                        traits)...),
            base::trait_helpers::HasTrait<RealMainThreadScheduler,
                                          Traits...>()) {}

  ~TaskEnvironmentImpl() override;

  scheduler::MainThreadSchedulerImpl* main_thread_scheduler() {
    return scheduler_.get();
  }
  v8::Isolate* isolate() { return main_thread_isolate_->isolate(); }

  static bool IsSupported();
  static void SetSupported(bool is_supported);

 private:
  // When |real_main_thread_scheduler|, instantiate a full featured
  // blink::MainThreadScheduler as opposed to a simple Thread scheduler.
  TaskEnvironmentImpl(base::test::TaskEnvironment&& scoped_task_environment,
                      bool real_main_thread_scheduler);

  std::unique_ptr<scheduler::MainThreadSchedulerImpl> scheduler_;
  absl::optional<MainThreadIsolate> main_thread_isolate_;
  absl::optional<ScopedMainThreadOverrider> main_thread_overrider_;
};

}  // namespace internal

// TaskEnvironment is a convenience class which allows usage of these
// APIs within its scope:
// - Same APIs as base::test::TaskEnvironment.
// - Blink Main Thread isolate.
// - blink::scheduler::WebThreadScheduler.
//
// Only tests that need blink APIs should instantiate a
// blink::test::TaskEnvironment. Use base::test::SingleThreadTaskEnvironment or
// base::test::TaskEnvironment otherwise.
class TaskEnvironment {
 public:
  using RealMainThreadScheduler =
      internal::TaskEnvironmentImpl::RealMainThreadScheduler;
  using ValidTraits = internal::TaskEnvironmentImpl::ValidTraits;

  template <typename... Traits>
    requires base::trait_helpers::AreValidTraits<ValidTraits, Traits...>
  explicit TaskEnvironment(Traits... traits)
      : impl_{
            internal::TaskEnvironmentImpl::IsSupported()
                ? absl::make_optional<internal::TaskEnvironmentImpl>(traits...)
                : absl::nullopt} {}

  explicit operator bool() const { return impl_.has_value(); }
  internal::TaskEnvironmentImpl* operator->() { return impl_.operator->(); }
  internal::TaskEnvironmentImpl& operator*() { return *impl_; }

  v8::Isolate* isolate();

 private:
  absl::optional<internal::TaskEnvironmentImpl> impl_;
};

}  // namespace blink::test

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_TASK_ENVIRONMENT_H_
