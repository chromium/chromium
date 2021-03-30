// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scripted_idle_task_controller.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_options.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/scoped_scheduler_overrider.h"

namespace blink {
namespace {

enum class ShouldYield { YIELD, DONT_YIELD };

class MockScriptedIdleTaskControllerScheduler final : public ThreadScheduler {
 public:
  explicit MockScriptedIdleTaskControllerScheduler(ShouldYield should_yield)
      : should_yield_(should_yield == ShouldYield::YIELD) {}
  MockScriptedIdleTaskControllerScheduler(
      const MockScriptedIdleTaskControllerScheduler&) = delete;
  MockScriptedIdleTaskControllerScheduler& operator=(
      const MockScriptedIdleTaskControllerScheduler&) = delete;
  ~MockScriptedIdleTaskControllerScheduler() override = default;

  // ThreadScheduler implementation:
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override {
    return nullptr;
  }
  scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() override {
    return nullptr;
  }
  scoped_refptr<base::SingleThreadTaskRunner> NonWakingTaskRunner() override {
    return nullptr;
  }
  scoped_refptr<base::SingleThreadTaskRunner> DeprecatedDefaultTaskRunner()
      override {
    return task_runner_;
  }
  void Shutdown() override {}
  bool ShouldYieldForHighPriorityWork() override { return should_yield_; }
  bool CanExceedIdleDeadlineIfRequired() const override { return false; }
  void PostIdleTask(const base::Location&,
                    Thread::IdleTask idle_task) override {
    idle_task_ = std::move(idle_task);
  }
  void PostDelayedIdleTask(const base::Location&,
                           base::TimeDelta,
                           Thread::IdleTask) override {
    NOTIMPLEMENTED();
  }
  void PostNonNestableIdleTask(const base::Location&,
                               Thread::IdleTask) override {}
  std::unique_ptr<scheduler::WebAgentGroupScheduler> CreateAgentGroupScheduler()
      override {
    NOTREACHED();
    return nullptr;
  }
  std::unique_ptr<RendererPauseHandle> PauseScheduler() override {
    return nullptr;
  }
  scheduler::WebAgentGroupScheduler* GetCurrentAgentGroupScheduler() override {
    NOTREACHED();
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

  void RunIdleTask() { std::move(idle_task_).Run(base::TimeTicks()); }
  bool HasIdleTask() const { return !!idle_task_; }

  void AdvanceTimeAndRun(base::TimeDelta delta) {
    task_runner_->AdvanceTimeAndRun(delta);
  }

 private:
  bool should_yield_;
  Thread::IdleTask idle_task_;
  scoped_refptr<scheduler::FakeTaskRunner> task_runner_ =
      base::MakeRefCounted<scheduler::FakeTaskRunner>();
};

class MockIdleTask : public IdleTask {
 public:
  MOCK_METHOD1(invoke, void(IdleDeadline*));
};
}  // namespace

class ScriptedIdleTaskControllerTest : public testing::Test {
 public:
  ~ScriptedIdleTaskControllerTest() override {
    execution_context_->NotifyContextDestroyed();
  }

  void SetUp() override {
    execution_context_ = MakeGarbageCollected<NullExecutionContext>();
  }

 protected:
  Persistent<ExecutionContext> execution_context_;
};

TEST_F(ScriptedIdleTaskControllerTest, RunCallback) {
  MockScriptedIdleTaskControllerScheduler scheduler(ShouldYield::DONT_YIELD);
  ScopedSchedulerOverrider scheduler_overrider(&scheduler);

  ScriptedIdleTaskController* controller =
      ScriptedIdleTaskController::Create(execution_context_);

  Persistent<MockIdleTask> idle_task(MakeGarbageCollected<MockIdleTask>());
  IdleRequestOptions* options = IdleRequestOptions::Create();
  EXPECT_FALSE(scheduler.HasIdleTask());
  int id = controller->RegisterCallback(idle_task, options);
  EXPECT_TRUE(scheduler.HasIdleTask());
  EXPECT_NE(0, id);

  EXPECT_CALL(*idle_task, invoke(testing::_));
  scheduler.RunIdleTask();
  testing::Mock::VerifyAndClearExpectations(idle_task);
  EXPECT_FALSE(scheduler.HasIdleTask());
}

TEST_F(ScriptedIdleTaskControllerTest, DontRunCallbackWhenAskedToYield) {
  MockScriptedIdleTaskControllerScheduler scheduler(ShouldYield::YIELD);
  ScopedSchedulerOverrider scheduler_overrider(&scheduler);

  ScriptedIdleTaskController* controller =
      ScriptedIdleTaskController::Create(execution_context_);

  Persistent<MockIdleTask> idle_task(MakeGarbageCollected<MockIdleTask>());
  IdleRequestOptions* options = IdleRequestOptions::Create();
  int id = controller->RegisterCallback(idle_task, options);
  EXPECT_NE(0, id);

  EXPECT_CALL(*idle_task, invoke(testing::_)).Times(0);
  scheduler.RunIdleTask();
  testing::Mock::VerifyAndClearExpectations(idle_task);

  // The idle task should have been reposted.
  EXPECT_TRUE(scheduler.HasIdleTask());
}

TEST_F(ScriptedIdleTaskControllerTest, RunCallbacksAsyncWhenUnpaused) {
  MockScriptedIdleTaskControllerScheduler scheduler(ShouldYield::YIELD);
  ScopedSchedulerOverrider scheduler_overrider(&scheduler);
  ScriptedIdleTaskController* controller =
      ScriptedIdleTaskController::Create(execution_context_);

  // Register an idle task with a deadline.
  Persistent<MockIdleTask> idle_task(MakeGarbageCollected<MockIdleTask>());
  IdleRequestOptions* options = IdleRequestOptions::Create();
  options->setTimeout(1);
  int id = controller->RegisterCallback(idle_task, options);
  EXPECT_NE(0, id);

  // Hitting the deadline while the frame is paused shouldn't cause any tasks to
  // run.
  controller->ContextLifecycleStateChanged(mojom::FrameLifecycleState::kPaused);
  EXPECT_CALL(*idle_task, invoke(testing::_)).Times(0);
  scheduler.AdvanceTimeAndRun(base::TimeDelta::FromMilliseconds(1));
  testing::Mock::VerifyAndClearExpectations(idle_task);

  // Even if we unpause, no tasks should run immediately.
  EXPECT_CALL(*idle_task, invoke(testing::_)).Times(0);
  controller->ContextLifecycleStateChanged(
      mojom::FrameLifecycleState::kRunning);
  testing::Mock::VerifyAndClearExpectations(idle_task);

  // Idle callback should have been scheduled as an asynchronous task.
  EXPECT_CALL(*idle_task, invoke(testing::_)).Times(1);
  scheduler.AdvanceTimeAndRun(base::TimeDelta::FromMilliseconds(0));
  testing::Mock::VerifyAndClearExpectations(idle_task);
}

}  // namespace blink
