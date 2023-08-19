// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scripted_idle_task_controller.h"

#include "base/task/single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_options.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/scoped_scheduler_overrider.h"

namespace blink {
namespace {

using ShouldYield = base::StrongAlias<class ShouldYieldTag, bool>;

class MockScriptedIdleTaskControllerScheduler final : public ThreadScheduler {
 public:
  explicit MockScriptedIdleTaskControllerScheduler(ShouldYield should_yield)
      : should_yield_(should_yield) {}
  MockScriptedIdleTaskControllerScheduler(
      const MockScriptedIdleTaskControllerScheduler&) = delete;
  MockScriptedIdleTaskControllerScheduler& operator=(
      const MockScriptedIdleTaskControllerScheduler&) = delete;
  ~MockScriptedIdleTaskControllerScheduler() override = default;

  // ThreadScheduler implementation:
  scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() override {
    return nullptr;
  }
  scoped_refptr<base::SingleThreadTaskRunner> CleanupTaskRunner() override {
    return nullptr;
  }
  void Shutdown() override {}
  bool ShouldYieldForHighPriorityWork() override { return should_yield_; }
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
  base::TimeTicks MonotonicallyIncreasingVirtualTime() override {
    return base::TimeTicks();
  }

  void AddTaskObserver(Thread::TaskObserver* task_observer) override {}

  void RemoveTaskObserver(Thread::TaskObserver* task_observer) override {}

  void SetV8Isolate(v8::Isolate* isolate) override {}

  void RunIdleTask() { std::move(idle_task_).Run(base::TimeTicks()); }
  bool HasIdleTask() const { return !!idle_task_; }
  scoped_refptr<base::SingleThreadTaskRunner> TaskRunner() {
    return task_runner_;
  }

  void AdvanceTimeAndRun(base::TimeDelta delta) {
    task_runner_->AdvanceTimeAndRun(delta);
  }

 private:
  bool should_yield_;
  Thread::IdleTask idle_task_;
  scoped_refptr<scheduler::FakeTaskRunner> task_runner_ =
      base::MakeRefCounted<scheduler::FakeTaskRunner>();
};

class IdleTaskControllerFrameScheduler : public FrameScheduler {
 public:
  explicit IdleTaskControllerFrameScheduler(
      MockScriptedIdleTaskControllerScheduler* scripted_idle_scheduler)
      : scripted_idle_scheduler_(scripted_idle_scheduler),
        page_scheduler_(scheduler::CreateDummyPageScheduler()) {}
  ~IdleTaskControllerFrameScheduler() override = default;

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType) override {
    DCHECK(WTF::IsMainThread());
    return scripted_idle_scheduler_->TaskRunner();
  }

  PageScheduler* GetPageScheduler() const override {
    return page_scheduler_.get();
  }
  AgentGroupScheduler* GetAgentGroupScheduler() override {
    return &page_scheduler_->GetAgentGroupScheduler();
  }

  void SetPreemptedForCooperativeScheduling(Preempted) override {}
  void SetFrameVisible(bool) override {}
  bool IsFrameVisible() const override { return true; }
  bool IsPageVisible() const override { return true; }
  void SetPaused(bool) override {}
  void SetShouldReportPostedTasksWhenDisabled(bool) override {}
  void SetCrossOriginToNearestMainFrame(bool) override {}
  bool IsCrossOriginToNearestMainFrame() const override { return false; }
  void SetIsAdFrame(bool is_ad_frame) override {}
  bool IsAdFrame() const override { return false; }
  bool IsInEmbeddedFrameTree() const override { return false; }
  void TraceUrlChange(const String&) override {}
  void AddTaskTime(base::TimeDelta) override {}
  FrameType GetFrameType() const override { return FrameType::kMainFrame; }
  WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      const String& name,
      WebScopedVirtualTimePauser::VirtualTaskDuration) override {
    return WebScopedVirtualTimePauser();
  }
  void DidStartProvisionalLoad() override {}
  void DidCommitProvisionalLoad(bool,
                                FrameScheduler::NavigationType,
                                DidCommitProvisionalLoadParams) override {}
  void OnFirstContentfulPaintInMainFrame() override {}
  void OnMainFrameInteractive() override {}
  void OnFirstMeaningfulPaint() override {}
  bool IsExemptFromBudgetBasedThrottling() const override { return false; }
  std::unique_ptr<blink::mojom::blink::PauseSubresourceLoadingHandle>
  GetPauseSubresourceLoadingHandle() override {
    return nullptr;
  }
  std::unique_ptr<WebSchedulingTaskQueue> CreateWebSchedulingTaskQueue(
      WebSchedulingQueueType,
      WebSchedulingPriority) override {
    return nullptr;
  }
  ukm::SourceId GetUkmSourceId() override { return ukm::kInvalidSourceId; }
  void OnStartedUsingNonStickyFeature(
      SchedulingPolicy::Feature feature,
      const SchedulingPolicy& policy,
      std::unique_ptr<SourceLocation> source_location,
      SchedulingAffectingFeatureHandle* handle) override {}
  void OnStartedUsingStickyFeature(
      SchedulingPolicy::Feature feature,
      const SchedulingPolicy& policy,
      std::unique_ptr<SourceLocation> source_location) override {}
  void OnStoppedUsingNonStickyFeature(
      SchedulingAffectingFeatureHandle* handle) override {}
  base::WeakPtr<FrameOrWorkerScheduler> GetFrameOrWorkerSchedulerWeakPtr()
      override {
    return weak_ptr_factory_.GetWeakPtr();
  }
  WTF::HashSet<SchedulingPolicy::Feature>
  GetActiveFeaturesTrackedForBackForwardCacheMetrics() override {
    return WTF::HashSet<SchedulingPolicy::Feature>();
  }
  base::WeakPtr<FrameScheduler> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }
  void ReportActiveSchedulerTrackedFeatures() override {}
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override {
    return scripted_idle_scheduler_->TaskRunner();
  }

 private:
  MockScriptedIdleTaskControllerScheduler* scripted_idle_scheduler_;
  std::unique_ptr<PageScheduler> page_scheduler_;
  base::WeakPtrFactory<FrameScheduler> weak_ptr_factory_{this};
};

class MockIdleTask : public IdleTask {
 public:
  MOCK_METHOD1(invoke, void(IdleDeadline*));
};
}  // namespace

TEST(ScriptedIdleTaskControllerTest, RunCallback) {
  MockScriptedIdleTaskControllerScheduler scheduler(ShouldYield(false));
  ScopedSchedulerOverrider scheduler_overrider(&scheduler,
                                               scheduler.TaskRunner());
  ScopedNullExecutionContext execution_context(
      std::make_unique<IdleTaskControllerFrameScheduler>(&scheduler));
  ScriptedIdleTaskController* controller = ScriptedIdleTaskController::Create(
      &execution_context.GetExecutionContext());

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

TEST(ScriptedIdleTaskControllerTest, DontRunCallbackWhenAskedToYield) {
  MockScriptedIdleTaskControllerScheduler scheduler(ShouldYield(true));
  ScopedSchedulerOverrider scheduler_overrider(&scheduler,
                                               scheduler.TaskRunner());
  ScopedNullExecutionContext execution_context(
      std::make_unique<IdleTaskControllerFrameScheduler>(&scheduler));
  ScriptedIdleTaskController* controller = ScriptedIdleTaskController::Create(
      &execution_context.GetExecutionContext());

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

TEST(ScriptedIdleTaskControllerTest, RunCallbacksAsyncWhenUnpaused) {
  MockScriptedIdleTaskControllerScheduler scheduler(ShouldYield(true));
  ScopedSchedulerOverrider scheduler_overrider(&scheduler,
                                               scheduler.TaskRunner());
  ScopedNullExecutionContext execution_context(
      std::make_unique<IdleTaskControllerFrameScheduler>(&scheduler));
  ScriptedIdleTaskController* controller = ScriptedIdleTaskController::Create(
      &execution_context.GetExecutionContext());

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
  scheduler.AdvanceTimeAndRun(base::Milliseconds(1));
  testing::Mock::VerifyAndClearExpectations(idle_task);

  // Even if we unpause, no tasks should run immediately.
  EXPECT_CALL(*idle_task, invoke(testing::_)).Times(0);
  controller->ContextLifecycleStateChanged(
      mojom::FrameLifecycleState::kRunning);
  testing::Mock::VerifyAndClearExpectations(idle_task);

  // Idle callback should have been scheduled as an asynchronous task.
  EXPECT_CALL(*idle_task, invoke(testing::_)).Times(1);
  scheduler.AdvanceTimeAndRun(base::Milliseconds(0));
  testing::Mock::VerifyAndClearExpectations(idle_task);
}

}  // namespace blink
