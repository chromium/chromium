// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/idle_deadline.h"

#include "base/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/scoped_scheduler_overrider.h"

namespace blink {
namespace {

class MockIdleDeadlineScheduler final : public ThreadScheduler {
 public:
  MockIdleDeadlineScheduler() = default;
  ~MockIdleDeadlineScheduler() override = default;

  // ThreadScheduler implementation:
  scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() override {
    return nullptr;
  }
  void Shutdown() override {}
  bool ShouldYieldForHighPriorityWork() override { return true; }
  bool CanExceedIdleDeadlineIfRequired() const override { return false; }
  void PostIdleTask(const base::Location&, Thread::IdleTask) override {}
  void PostDelayedIdleTask(const base::Location&,
                           base::TimeDelta,
                           Thread::IdleTask) override {}
  void PostNonNestableIdleTask(const base::Location&,
                               Thread::IdleTask) override {}
  std::unique_ptr<PageScheduler> CreatePageScheduler(
      PageScheduler::Delegate*) override {
    return nullptr;
  }
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override {
    return nullptr;
  }
  scoped_refptr<base::SingleThreadTaskRunner> IPCTaskRunner() override {
    return nullptr;
  }
  scoped_refptr<base::SingleThreadTaskRunner> DeprecatedDefaultTaskRunner()
      override {
    return nullptr;
  }
  std::unique_ptr<RendererPauseHandle> PauseScheduler() override {
    return nullptr;
  }

  base::TimeTicks MonotonicallyIncreasingVirtualTime() override {
    return base::TimeTicks();
  }

  void AddTaskObserver(Thread::TaskObserver* task_observer) override {}

  void RemoveTaskObserver(Thread::TaskObserver* task_observer) override {}

  void AddRAILModeObserver(RAILModeObserver*) override {}

  void RemoveRAILModeObserver(RAILModeObserver const*) override {}

  scheduler::NonMainThreadSchedulerImpl* AsNonMainThreadScheduler() override {
    return nullptr;
  }

  void SetV8Isolate(v8::Isolate* isolate) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIdleDeadlineScheduler);
};

}  // namespace

class IdleDeadlineTest : public testing::Test {
 public:
  void SetUp() override {
    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
};

TEST_F(IdleDeadlineTest, DeadlineInFuture) {
  auto* deadline = MakeGarbageCollected<IdleDeadline>(
      base::TimeTicks() + base::TimeDelta::FromSecondsD(1.25),
      IdleDeadline::CallbackType::kCalledWhenIdle);
  deadline->SetTickClockForTesting(test_task_runner_->GetMockTickClock());
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  // Note: the deadline is computed with reduced resolution.
  EXPECT_FLOAT_EQ(250.0, deadline->timeRemaining());
}

TEST_F(IdleDeadlineTest, DeadlineInPast) {
  auto* deadline = MakeGarbageCollected<IdleDeadline>(
      base::TimeTicks() + base::TimeDelta::FromSecondsD(0.75),
      IdleDeadline::CallbackType::kCalledWhenIdle);
  deadline->SetTickClockForTesting(test_task_runner_->GetMockTickClock());
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FLOAT_EQ(0, deadline->timeRemaining());
}

TEST_F(IdleDeadlineTest, YieldForHighPriorityWork) {
  MockIdleDeadlineScheduler scheduler;
  ScopedSchedulerOverrider scheduler_overrider(&scheduler);

  auto* deadline = MakeGarbageCollected<IdleDeadline>(
      base::TimeTicks() + base::TimeDelta::FromSecondsD(1.25),
      IdleDeadline::CallbackType::kCalledWhenIdle);
  deadline->SetTickClockForTesting(test_task_runner_->GetMockTickClock());
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_FLOAT_EQ(0, deadline->timeRemaining());
}

}  // namespace blink
