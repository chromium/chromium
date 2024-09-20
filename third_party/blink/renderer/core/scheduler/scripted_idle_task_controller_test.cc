// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/scripted_idle_task_controller.h"

#include <deque>

#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
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
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/scoped_scheduler_overrider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

using ShouldYield = base::StrongAlias<class ShouldYieldTag, bool>;

// A facade to a real DelayedTaskHandle instance that hooks CancelTask() call.
class DelayedTaskHandleDelegateFacade
    : public base::DelayedTaskHandle::Delegate {
 public:
  explicit DelayedTaskHandleDelegateFacade(base::DelayedTaskHandle handle,
                                           base::OnceClosure on_canceled)
      : handle_(std::move(handle)), on_canceled_(std::move(on_canceled)) {}
  ~DelayedTaskHandleDelegateFacade() override = default;

  bool IsValid() const override { return handle_.IsValid(); }

  void CancelTask() override {
    if (IsValid()) {
      std::move(on_canceled_).Run();
    }
    handle_.CancelTask();
  }

 private:
  base::DelayedTaskHandle handle_;
  base::OnceClosure on_canceled_;
};

// A variant of `FakeTaskRunner` that counts the number of cancelled tasks.
class TestTaskRunner : public scheduler::FakeTaskRunner {
 public:
  int GetTaskCanceledCount() const { return task_canceled_count_; }

 private:
  base::DelayedTaskHandle PostCancelableDelayedTask(
      base::subtle::PostDelayedTaskPassKey pass_key,
      const base::Location& from_here,
      base::OnceClosure task,
      base::TimeDelta delay) override {
    auto handle = scheduler::FakeTaskRunner::PostCancelableDelayedTask(
        pass_key, from_here, std::move(task), delay);
    return base::DelayedTaskHandle(
        std::make_unique<DelayedTaskHandleDelegateFacade>(
            std::move(handle),
            base::BindOnce(&TestTaskRunner::OnTaskCanceled, this)));
  }

  void OnTaskCanceled() { ++task_canceled_count_; }

  int task_canceled_count_ = 0;
};
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
    idle_tasks_.push_back(std::move(idle_task));
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

  void SetV8Isolate(v8::Isolate* isolate) override { isolate_ = isolate; }

  void RunIdleTask() { TakeIdleTask().Run(base::TimeTicks()); }
  size_t GetNumIdleTasks() const { return idle_tasks_.size(); }
  Thread::IdleTask TakeIdleTask() {
    CHECK(!idle_tasks_.empty());
    auto idle_task = std::move(idle_tasks_.front());
    idle_tasks_.pop_front();
    return idle_task;
  }

  scoped_refptr<TestTaskRunner> TaskRunner() { return task_runner_; }

  void AdvanceTimeAndRun(base::TimeDelta delta) {
    task_runner_->AdvanceTimeAndRun(delta);
  }

  v8::Isolate* GetIsolate() { return isolate_; }

 private:
  v8::Isolate* isolate_;
  bool should_yield_;
  std::deque<Thread::IdleTask> idle_tasks_;
  scoped_refptr<TestTaskRunner> task_runner_ =
      base::MakeRefCounted<TestTaskRunner>();
};

class IdleTaskControllerFrameScheduler : public FrameScheduler {
 public:
  explicit IdleTaskControllerFrameScheduler(
      MockScriptedIdleTaskControllerScheduler* scripted_idle_scheduler)
      : scripted_idle_scheduler_(scripted_idle_scheduler),
        page_scheduler_(scheduler::CreateDummyPageScheduler(
            scripted_idle_scheduler->GetIsolate())) {}
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
  void SetVisibleAreaLarge(bool) override {}
  void SetHadUserActivation(bool) override {}
  bool IsPageVisible() const override { return true; }
  void SetPaused(bool) override {}
  void SetShouldReportPostedTasksWhenDisabled(bool) override {}
  void SetCrossOriginToNearestMainFrame(bool) override {}
  bool IsCrossOriginToNearestMainFrame() const override { return false; }
  void SetAgentClusterId(const base::UnguessableToken&) override {}
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
  void OnFirstMeaningfulPaint(base::TimeTicks timestamp) override {}
  void OnDispatchLoadEvent() override {}
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
  base::TimeDelta UnreportedTaskTime() const override {
    return base::TimeDelta();
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

class ScriptedIdleTaskControllerTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  ScriptedIdleTaskControllerTest() {
    if (IsOOMFixEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          kScriptedIdleTaskControllerOOMFix);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          kScriptedIdleTaskControllerOOMFix);
    }
  }

  void InitializeScheduler(ShouldYield should_yield) {
    scheduler_.emplace(should_yield);
    scheduler_overrider_.emplace(&scheduler_.value(), scheduler_->TaskRunner());
    execution_context_.emplace(
        std::make_unique<IdleTaskControllerFrameScheduler>(
            &scheduler_.value()));
  }

  void DeleteScheduler() {
    execution_context_.reset();
    scheduler_overrider_.reset();
    scheduler_.reset();
  }

  ScriptedIdleTaskController* GetController() {
    return &ScriptedIdleTaskController::From(
        execution_context_->GetExecutionContext());
  }

  bool IsOOMFixEnabled() { return GetParam(); }

 protected:
  test::TaskEnvironment task_environment_;
  std::optional<MockScriptedIdleTaskControllerScheduler> scheduler_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<ScopedSchedulerOverrider> scheduler_overrider_;
  std::optional<ScopedNullExecutionContext> execution_context_;
};

TEST_P(ScriptedIdleTaskControllerTest, RunCallback) {
  InitializeScheduler(ShouldYield(false));

  Persistent<MockIdleTask> idle_task(MakeGarbageCollected<MockIdleTask>());
  IdleRequestOptions* options = IdleRequestOptions::Create();
  EXPECT_EQ(0u, scheduler_->GetNumIdleTasks());
  int id = GetController()->RegisterCallback(idle_task, options);
  EXPECT_NE(id, 0);
  EXPECT_EQ(1u, scheduler_->GetNumIdleTasks());

  EXPECT_CALL(*idle_task, invoke(testing::_));
  scheduler_->RunIdleTask();
  testing::Mock::VerifyAndClearExpectations(idle_task);
  EXPECT_EQ(0u, scheduler_->GetNumIdleTasks());
}

TEST_P(ScriptedIdleTaskControllerTest, DontRunCallbackWhenAskedToYield) {
  InitializeScheduler(ShouldYield(true));

  Persistent<MockIdleTask> idle_task(MakeGarbageCollected<MockIdleTask>());
  IdleRequestOptions* options = IdleRequestOptions::Create();
  int id = GetController()->RegisterCallback(idle_task, options);
  EXPECT_NE(0, id);

  EXPECT_CALL(*idle_task, invoke(testing::_)).Times(0);
  scheduler_->RunIdleTask();
  testing::Mock::VerifyAndClearExpectations(idle_task);

  // The idle task should have been reposted.
  EXPECT_EQ(1u, scheduler_->GetNumIdleTasks());
}

TEST_P(ScriptedIdleTaskControllerTest, LongTimeoutShouldBeRemoveFromQueue) {
  InitializeScheduler(ShouldYield(false));

  // Register an idle task with a deadline.
  Persistent<MockIdleTask> idle_task(MakeGarbageCollected<MockIdleTask>());
  IdleRequestOptions* options = IdleRequestOptions::Create();
  options->setTimeout(1000000);
  int id = GetController()->RegisterCallback(idle_task, options);
  EXPECT_NE(id, 0);
  EXPECT_EQ(scheduler_->TaskRunner()->GetTaskCanceledCount(), 0);

  // Run the task.
  EXPECT_CALL(*idle_task, invoke(testing::_));
  scheduler_->RunIdleTask();
  testing::Mock::VerifyAndClearExpectations(idle_task);

  // The timeout task should be removed from the task queue.
  // Failure to do so is likely to result in OOM.
  EXPECT_EQ(scheduler_->TaskRunner()->GetTaskCanceledCount(), 1);
}

TEST_P(ScriptedIdleTaskControllerTest, RunAfterSchedulerWasDeleted) {
  InitializeScheduler(ShouldYield(false));

  scoped_refptr<TestTaskRunner> task_runner = scheduler_->TaskRunner();

  Persistent<MockIdleTask> idle_task(MakeGarbageCollected<MockIdleTask>());
  IdleRequestOptions* options = IdleRequestOptions::Create();
  options->setTimeout(1);

    // Register an idle task with a deadline.
  int id = GetController()->RegisterCallback(idle_task, options);
  EXPECT_NE(id, 0);

  Thread::IdleTask thread_idle_task = scheduler_->TakeIdleTask();

  DeleteScheduler();

  EXPECT_CALL(*idle_task, invoke(testing::_)).Times(0);
  std::move(thread_idle_task).Run(base::TimeTicks());
  testing::Mock::VerifyAndClearExpectations(idle_task);

  EXPECT_EQ(task_runner->GetTaskCanceledCount(), 1);
}

TEST_P(ScriptedIdleTaskControllerTest, NoUnnecessaryRepostOnUnpause) {
  InitializeScheduler(ShouldYield(false));

  // Register an idle task.
  Persistent<MockIdleTask> idle_task(MakeGarbageCollected<MockIdleTask>());
  GetController()->RegisterCallback(idle_task, IdleRequestOptions::Create());

  // Pause/unpause the context a few times.
  for (int i = 0; i < 3; ++i) {
    GetController()->ContextLifecycleStateChanged(
        mojom::FrameLifecycleState::kPaused);
    GetController()->ContextLifecycleStateChanged(
        mojom::FrameLifecycleState::kRunning);
  }

  // Pausing/unpausing the context should not cause more scheduler idle tasks to
  // be posted. That would unnecessarily use memory.
  if (IsOOMFixEnabled()) {
    EXPECT_EQ(scheduler_->GetNumIdleTasks(), 1u);
  } else {
    EXPECT_GT(scheduler_->GetNumIdleTasks(), 1u);
  }
}

TEST_P(ScriptedIdleTaskControllerTest,
       SchedulerTimeoutTaskCanceledOnIdleTaskCanceled) {
  InitializeScheduler(ShouldYield(false));

  // Register and cancel an idle task with a timeout.
  Persistent<MockIdleTask> idle_task(MakeGarbageCollected<MockIdleTask>());
  IdleRequestOptions* options = IdleRequestOptions::Create();
  options->setTimeout(1);
  const int id = GetController()->RegisterCallback(idle_task, options);
  GetController()->CancelCallback(id);

  // The scheduler timeout task should be canceled. Otherwise, it stays in the
  // queue until the timeout expires which unnecessarily uses memory.
  if (IsOOMFixEnabled()) {
    EXPECT_EQ(scheduler_->TaskRunner()->GetTaskCanceledCount(), 1);
  } else {
    EXPECT_EQ(scheduler_->TaskRunner()->GetTaskCanceledCount(), 0);
  }
}

INSTANTIATE_TEST_SUITE_P(, ScriptedIdleTaskControllerTest, ::testing::Bool());

}  // namespace blink
