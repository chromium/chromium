// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/idle_deadline.h"

#include "base/task/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/scoped_scheduler_overrider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

class MockIdleDeadlineScheduler final : public ThreadScheduler {
 public:
  MockIdleDeadlineScheduler() = default;
  MockIdleDeadlineScheduler(const MockIdleDeadlineScheduler&) = delete;
  MockIdleDeadlineScheduler& operator=(const MockIdleDeadlineScheduler&) =
      delete;
  ~MockIdleDeadlineScheduler() override = default;

  // ThreadScheduler implementation:
  scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() override {
    return nullptr;
  }
  scoped_refptr<base::SingleThreadTaskRunner> CleanupTaskRunner() override {
    return nullptr;
  }
  void Shutdown() override {}
  bool ShouldYieldForHighPriorityWork() override { return true; }
  void PostIdleTask(const base::Location&, Thread::IdleTask) override {}
  void PostDelayedIdleTask(const base::Location&,
                           base::TimeDelta,
                           Thread::IdleTask) override {}
  void PostNonNestableIdleTask(const base::Location&,
                               Thread::IdleTask) override {}

  base::TimeTicks MonotonicallyIncreasingVirtualTime() override {
    return base::TimeTicks();
  }

  void AddTaskObserver(Thread::TaskObserver* task_observer) override {}

  void RemoveTaskObserver(Thread::TaskObserver* task_observer) override {}

  void SetV8Isolate(v8::Isolate* isolate) override {}
};

}  // namespace

class IdleDeadlineTest : public testing::Test {
 public:
  void SetUp() override {
    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  }

 protected:
  test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
};

TEST_F(IdleDeadlineTest, DeadlineInFuture) {
  auto* deadline = MakeGarbageCollected<IdleDeadline>(
      base::TimeTicks() + base::Seconds(1.25),
      /*cross_origin_isolated_capability=*/false,
      IdleDeadline::CallbackType::kCalledWhenIdle);
  deadline->SetTickClockForTesting(test_task_runner_->GetMockTickClock());
  test_task_runner_->FastForwardBy(base::Seconds(1));
  // Note: the deadline is computed with reduced resolution.
  EXPECT_FLOAT_EQ(250.0, deadline->timeRemaining());
}

TEST_F(IdleDeadlineTest, DeadlineInPast) {
  auto* deadline = MakeGarbageCollected<IdleDeadline>(
      base::TimeTicks() + base::Seconds(0.75),
      /*cross_origin_isolated_capability=*/false,
      IdleDeadline::CallbackType::kCalledWhenIdle);
  deadline->SetTickClockForTesting(test_task_runner_->GetMockTickClock());
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FLOAT_EQ(0, deadline->timeRemaining());
}

TEST_F(IdleDeadlineTest, YieldForHighPriorityWork) {
  MockIdleDeadlineScheduler scheduler;
  ScopedSchedulerOverrider scheduler_overrider(&scheduler, test_task_runner_);

  auto* deadline = MakeGarbageCollected<IdleDeadline>(
      base::TimeTicks() + base::Seconds(1.25),
      /*cross_origin_isolated_capability=*/false,
      IdleDeadline::CallbackType::kCalledWhenIdle);
  deadline->SetTickClockForTesting(test_task_runner_->GetMockTickClock());
  test_task_runner_->FastForwardBy(base::Seconds(1));
  EXPECT_FLOAT_EQ(0, deadline->timeRemaining());
}

}  // namespace blink
