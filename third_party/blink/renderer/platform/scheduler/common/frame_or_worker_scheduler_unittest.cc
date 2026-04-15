// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

#include <memory>
#include <utility>

#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_lifecycle_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink::scheduler {
namespace {

class FrameOrWorkerSchedulerForTest : public FrameOrWorkerScheduler {
 public:
  FrameOrWorkerSchedulerForTest() = default;
  ~FrameOrWorkerSchedulerForTest() override = default;

  void SetLifecycleState(SchedulingLifecycleState state) {
    lifecycle_state_ = state;
    NotifyLifecycleObservers();
  }

  std::unique_ptr<WebSchedulingTaskQueue> CreateWebSchedulingTaskQueue(
      WebSchedulingQueueType,
      WebSchedulingPriority) override {
    return nullptr;
  }

  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override {
    return nullptr;
  }

  WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      const String& name,
      WebScopedVirtualTimePauser::VirtualTaskDuration) override {
    return WebScopedVirtualTimePauser();
  }

  scheduler::SchedulingLifecycleState CalculateLifecycleState(
      ObserverType) const override {
    return lifecycle_state_;
  }

  void OnStartedUsingNonStickyFeature(
      SchedulingPolicy::Feature feature,
      const SchedulingPolicy& policy,
      SourceLocation* source_location,
      SchedulingAffectingFeatureHandle* handle) override {}

  void OnStartedUsingStickyFeature(SchedulingPolicy::Feature feature,
                                   const SchedulingPolicy& policy,
                                   SourceLocation* source_location) override {}

  void OnStoppedUsingNonStickyFeature(
      SchedulingAffectingFeatureHandle* handle) override {}

  base::WeakPtr<FrameOrWorkerScheduler> GetFrameOrWorkerSchedulerWeakPtr()
      override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  SchedulingLifecycleState lifecycle_state_ =
      SchedulingLifecycleState::kNotThrottled;

  base::WeakPtrFactory<FrameOrWorkerSchedulerForTest> weak_ptr_factory_{this};
};

class FrameOrWorkerSchedulerTest : public testing::Test {
 public:
  FrameOrWorkerSchedulerTest()
      : scheduler_(std::make_unique<FrameOrWorkerSchedulerForTest>()) {}

 protected:
  std::unique_ptr<FrameOrWorkerSchedulerForTest> scheduler_;
};

TEST_F(FrameOrWorkerSchedulerTest, LifecycleStateCallbacksRunsWhenExpected) {
  constexpr auto kObserverType = FrameOrWorkerScheduler::ObserverType::kLoader;
  std::optional<SchedulingLifecycleState> lifecycle_state;
  std::unique_ptr<FrameOrWorkerScheduler::LifecycleObserverHandle> handle(
      scheduler_->AddLifecycleObserver(
          kObserverType,
          base::BindLambdaForTesting([&](SchedulingLifecycleState state) {
            lifecycle_state = state;
          })));
  ASSERT_TRUE(lifecycle_state.has_value());
  EXPECT_EQ(*lifecycle_state,
            scheduler_->CalculateLifecycleState(kObserverType));

  lifecycle_state = std::nullopt;
  scheduler_->SetLifecycleState(SchedulingLifecycleState::kHidden);
  ASSERT_TRUE(lifecycle_state.has_value());
  EXPECT_EQ(*lifecycle_state,
            scheduler_->CalculateLifecycleState(kObserverType));

  lifecycle_state = std::nullopt;
  handle.reset();
  scheduler_->SetLifecycleState(SchedulingLifecycleState::kThrottled);
  ASSERT_FALSE(lifecycle_state.has_value());
}

TEST_F(FrameOrWorkerSchedulerTest,
       LifecycleStateCallbacksRemovedDuringNofification) {
  constexpr auto kObserverType = FrameOrWorkerScheduler::ObserverType::kLoader;
  Vector<std::unique_ptr<FrameOrWorkerScheduler::LifecycleObserverHandle>>
      handles;
  int count = 0;
  bool should_clear = false;
  for (int i = 0; i < 100; i++) {
    handles.emplace_back(scheduler_->AddLifecycleObserver(
        kObserverType,
        base::BindLambdaForTesting([&](SchedulingLifecycleState state) {
          ++count;
          if (should_clear) {
            handles.clear();
          }
        })));
  }
  EXPECT_EQ(count, 100);

  // Callbacks should not run for observers removed during iteration.
  should_clear = true;
  scheduler_->SetLifecycleState(SchedulingLifecycleState::kHidden);
  EXPECT_TRUE(handles.empty());
  EXPECT_EQ(count, 101);
}

}  // namespace
}  // namespace blink::scheduler
