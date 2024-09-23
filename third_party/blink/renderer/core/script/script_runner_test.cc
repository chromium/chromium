// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/script/script_runner.h"

#include "base/test/null_task_runner.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/script/mock_script_element_base.h"
#include "third_party/blink/renderer/core/script/pending_script.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

using testing::InvokeWithoutArgs;
using testing::ElementsAre;
using testing::Return;
using testing::WhenSorted;
using testing::ElementsAreArray;
using testing::_;

namespace blink {

class MockPendingScript : public PendingScript {
 public:
  static MockPendingScript* CreateInOrder(Document* document) {
    return Create(document, ScriptSchedulingType::kInOrder);
  }

  static MockPendingScript* CreateAsync(Document* document) {
    return Create(document, ScriptSchedulingType::kAsync);
  }

  MockPendingScript(ScriptElementBase* element,
                    ScriptSchedulingType scheduling_type)
      : PendingScript(element,
                      TextPosition::MinimumPosition(),
                      /*parent_task=*/nullptr) {
    SetSchedulingType(scheduling_type);
  }
  ~MockPendingScript() override {}

  MOCK_CONST_METHOD0(GetScriptType, mojom::blink::ScriptType());
  MOCK_CONST_METHOD1(CheckMIMETypeBeforeRunScript, bool(Document*));
  MOCK_CONST_METHOD0(GetSource, Script*());
  MOCK_CONST_METHOD0(IsExternal, bool());
  MOCK_CONST_METHOD0(WasCanceled, bool());
  MOCK_CONST_METHOD0(UrlForTracing, KURL());
  MOCK_METHOD0(RemoveFromMemoryCache, void());
  MOCK_METHOD0(ExecuteScriptBlock, void());

  bool IsReady() const override { return is_ready_; }
  void SetIsReady(bool is_ready) { is_ready_ = is_ready; }

 protected:
  MOCK_METHOD0(DisposeInternal, void());
  MOCK_CONST_METHOD0(CheckState, void());

 private:
  static MockPendingScript* Create(Document* document,
                                   ScriptSchedulingType scheduling_type) {
    MockScriptElementBase* element = MockScriptElementBase::Create();
    EXPECT_CALL(*element, GetDocument())
        .WillRepeatedly(testing::ReturnRef(*document));
    EXPECT_CALL(*element, GetExecutionContext())
        .WillRepeatedly(testing::Return(document->GetExecutionContext()));
    MockPendingScript* pending_script =
        MakeGarbageCollected<MockPendingScript>(element, scheduling_type);
    EXPECT_CALL(*pending_script, IsExternal()).WillRepeatedly(Return(true));
    return pending_script;
  }

  bool is_ready_ = false;
  base::OnceClosure streaming_finished_callback_;
};

class ScriptRunnerTest : public testing::Test {
 public:
  ScriptRunnerTest()
      : page_holder_(std::make_unique<DummyPageHolder>()),
        document_(&page_holder_->GetDocument()) {}

  void SetUp() override {
    script_runner_ = MakeGarbageCollected<ScriptRunner>(document_.Get());
    // Give ScriptRunner a task runner that platform_ will pump in
    // RunUntilIdle()/RunSingleTask().
    script_runner_->SetTaskRunnerForTesting(
        platform_->GetMainThreadScheduler()->DefaultTaskRunner().get());
    RuntimeCallStats::SetRuntimeCallStatsForTesting();
  }
  void TearDown() override {
    script_runner_.Release();
    RuntimeCallStats::ClearRuntimeCallStatsForTesting();
  }

 protected:
  void NotifyScriptReady(MockPendingScript* pending_script) {
    pending_script->SetIsReady(true);
    script_runner_->PendingScriptFinished(pending_script);
  }

  void QueueScriptForExecution(MockPendingScript* pending_script) {
    script_runner_->QueueScriptForExecution(
        pending_script, static_cast<ScriptRunner::DelayReasons>(
                            ScriptRunner::DelayReason::kLoad));
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  Persistent<Document> document_;
  Persistent<ScriptRunner> script_runner_;
  WTF::Vector<int> order_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
};

TEST_F(ScriptRunnerTest, QueueSingleScript_Async) {
  auto* pending_script = MockPendingScript::CreateAsync(document_);

  QueueScriptForExecution(pending_script);
  NotifyScriptReady(pending_script);

  EXPECT_CALL(*pending_script, ExecuteScriptBlock());
  platform_->RunUntilIdle();
}

TEST_F(ScriptRunnerTest, QueueSingleScript_InOrder) {
  auto* pending_script = MockPendingScript::CreateInOrder(document_);
  QueueScriptForExecution(pending_script);

  EXPECT_CALL(*pending_script, ExecuteScriptBlock());

  NotifyScriptReady(pending_script);

  platform_->RunUntilIdle();
}

TEST_F(ScriptRunnerTest, QueueMultipleScripts_InOrder) {
  auto* pending_script1 = MockPendingScript::CreateInOrder(document_);
  auto* pending_script2 = MockPendingScript::CreateInOrder(document_);
  auto* pending_script3 = MockPendingScript::CreateInOrder(document_);

  HeapVector<Member<MockPendingScript>> pending_scripts;
  pending_scripts.push_back(pending_script1);
  pending_scripts.push_back(pending_script2);
  pending_scripts.push_back(pending_script3);

  for (MockPendingScript* pending_script : pending_scripts) {
    QueueScriptForExecution(pending_script);
  }

  for (wtf_size_t i = 0; i < pending_scripts.size(); ++i) {
    EXPECT_CALL(*pending_scripts[i], ExecuteScriptBlock())
        .WillOnce(InvokeWithoutArgs([this, i] { order_.push_back(i + 1); }));
  }

  for (int i = 2; i >= 0; i--) {
    NotifyScriptReady(pending_scripts[i]);
    platform_->RunUntilIdle();
  }

  // But ensure the scripts were run in the expected order.
  EXPECT_THAT(order_, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, QueueMixedScripts) {
  auto* pending_script1 = MockPendingScript::CreateInOrder(document_);
  auto* pending_script2 = MockPendingScript::CreateInOrder(document_);
  auto* pending_script3 = MockPendingScript::CreateInOrder(document_);
  auto* pending_script4 = MockPendingScript::CreateAsync(document_);
  auto* pending_script5 = MockPendingScript::CreateAsync(document_);

  QueueScriptForExecution(pending_script1);
  QueueScriptForExecution(pending_script2);
  QueueScriptForExecution(pending_script3);
  QueueScriptForExecution(pending_script4);
  QueueScriptForExecution(pending_script5);

  NotifyScriptReady(pending_script1);
  NotifyScriptReady(pending_script3);
  NotifyScriptReady(pending_script5);

  EXPECT_CALL(*pending_script1, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(1); }));
  EXPECT_CALL(*pending_script2, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(2); }));
  EXPECT_CALL(*pending_script3, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(3); }));
  EXPECT_CALL(*pending_script4, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(4); }));
  EXPECT_CALL(*pending_script5, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(5); }));

  platform_->RunSingleTask();
  document_->domWindow()->SetLifecycleState(
      mojom::FrameLifecycleState::kPaused);
  document_->domWindow()->SetLifecycleState(
      mojom::FrameLifecycleState::kRunning);
  platform_->RunUntilIdle();

  // In-order script 3 cannot run, since in-order script 2 just scheduled before
  // is not yet ready.
  // Async scripts that are ready can skip the previously queued other async
  // scripts, so 5 runs.
  EXPECT_THAT(order_, ElementsAre(1, 5));

  NotifyScriptReady(pending_script2);
  NotifyScriptReady(pending_script4);
  platform_->RunUntilIdle();

  // In-order script 3 can now run.
  EXPECT_THAT(order_, ElementsAre(1, 5, 2, 3, 4));
}

TEST_F(ScriptRunnerTest, QueueReentrantScript_Async) {
  auto* pending_script1 = MockPendingScript::CreateAsync(document_);
  auto* pending_script2 = MockPendingScript::CreateAsync(document_);
  auto* pending_script3 = MockPendingScript::CreateAsync(document_);

  QueueScriptForExecution(pending_script1);
  QueueScriptForExecution(pending_script2);
  QueueScriptForExecution(pending_script3);
  NotifyScriptReady(pending_script1);

  auto* pending_script = pending_script2;
  EXPECT_CALL(*pending_script1, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([pending_script, this] {
        order_.push_back(1);
        NotifyScriptReady(pending_script);
      }));

  pending_script = pending_script3;
  EXPECT_CALL(*pending_script2, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([pending_script, this] {
        order_.push_back(2);
        NotifyScriptReady(pending_script);
      }));

  EXPECT_CALL(*pending_script3, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(3); }));

  // Make sure that re-entrant calls to notifyScriptReady don't cause
  // ScriptRunner::execute to do more work than expected.
  platform_->RunSingleTask();
  EXPECT_THAT(order_, ElementsAre(1));

  platform_->RunSingleTask();
  EXPECT_THAT(order_, ElementsAre(1, 2));

  platform_->RunSingleTask();
  EXPECT_THAT(order_, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, QueueReentrantScript_InOrder) {
  auto* pending_script1 = MockPendingScript::CreateInOrder(document_);
  auto* pending_script2 = MockPendingScript::CreateInOrder(document_);
  auto* pending_script3 = MockPendingScript::CreateInOrder(document_);

  QueueScriptForExecution(pending_script1);
  NotifyScriptReady(pending_script1);

  MockPendingScript* pending_script = pending_script2;
  EXPECT_CALL(*pending_script1, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([pending_script, &pending_script2, this] {
        order_.push_back(1);
        QueueScriptForExecution(pending_script);
        NotifyScriptReady(pending_script2);
      }));

  pending_script = pending_script3;
  EXPECT_CALL(*pending_script2, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([pending_script, &pending_script3, this] {
        order_.push_back(2);
        QueueScriptForExecution(pending_script);
        NotifyScriptReady(pending_script3);
      }));

  EXPECT_CALL(*pending_script3, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(3); }));

  // Make sure that re-entrant calls to queueScriptForExecution don't cause
  // ScriptRunner::execute to do more work than expected.
  platform_->RunSingleTask();
  EXPECT_THAT(order_, ElementsAre(1));

  platform_->RunSingleTask();
  EXPECT_THAT(order_, ElementsAre(1, 2));

  platform_->RunSingleTask();
  EXPECT_THAT(order_, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, QueueReentrantScript_ManyAsyncScripts) {
  MockPendingScript* pending_scripts[20];
  for (int i = 0; i < 20; i++)
    pending_scripts[i] = nullptr;

  for (int i = 0; i < 20; i++) {
    pending_scripts[i] = MockPendingScript::CreateAsync(document_);

    QueueScriptForExecution(pending_scripts[i]);

    if (i > 0) {
      EXPECT_CALL(*pending_scripts[i], ExecuteScriptBlock())
          .WillOnce(InvokeWithoutArgs([this, i] { order_.push_back(i); }));
    }
  }

  NotifyScriptReady(pending_scripts[0]);
  NotifyScriptReady(pending_scripts[1]);

  EXPECT_CALL(*pending_scripts[0], ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([&pending_scripts, this] {
        for (int i = 2; i < 20; i++) {
          NotifyScriptReady(pending_scripts[i]);
        }
        order_.push_back(0);
      }));

  platform_->RunUntilIdle();

  int expected[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
                    10, 11, 12, 13, 14, 15, 16, 17, 18, 19};

  EXPECT_THAT(order_, testing::ElementsAreArray(expected));
}

TEST_F(ScriptRunnerTest, ResumeAndSuspend_InOrder) {
  auto* pending_script1 = MockPendingScript::CreateInOrder(document_);
  auto* pending_script2 = MockPendingScript::CreateInOrder(document_);
  auto* pending_script3 = MockPendingScript::CreateInOrder(document_);

  QueueScriptForExecution(pending_script1);
  QueueScriptForExecution(pending_script2);
  QueueScriptForExecution(pending_script3);

  EXPECT_CALL(*pending_script1, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(1); }));
  EXPECT_CALL(*pending_script2, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(2); }));
  EXPECT_CALL(*pending_script3, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(3); }));

  NotifyScriptReady(pending_script1);
  NotifyScriptReady(pending_script2);
  NotifyScriptReady(pending_script3);

  document_->domWindow()->SetLifecycleState(
      mojom::FrameLifecycleState::kPaused);
  document_->domWindow()->SetLifecycleState(
      mojom::FrameLifecycleState::kRunning);
  platform_->RunUntilIdle();

  // Make sure elements are correct and in right order.
  EXPECT_THAT(order_, ElementsAre(1, 2, 3));
}

TEST_F(ScriptRunnerTest, ResumeAndSuspend_Async) {
  auto* pending_script1 = MockPendingScript::CreateAsync(document_);
  auto* pending_script2 = MockPendingScript::CreateAsync(document_);
  auto* pending_script3 = MockPendingScript::CreateAsync(document_);

  QueueScriptForExecution(pending_script1);
  QueueScriptForExecution(pending_script2);
  QueueScriptForExecution(pending_script3);

  NotifyScriptReady(pending_script1);
  NotifyScriptReady(pending_script2);
  NotifyScriptReady(pending_script3);

  EXPECT_CALL(*pending_script1, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(1); }));
  EXPECT_CALL(*pending_script2, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(2); }));
  EXPECT_CALL(*pending_script3, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(3); }));

  document_->domWindow()->SetLifecycleState(
      mojom::FrameLifecycleState::kPaused);
  document_->domWindow()->SetLifecycleState(
      mojom::FrameLifecycleState::kRunning);
  platform_->RunUntilIdle();

  // Make sure elements are correct.
  EXPECT_THAT(order_, WhenSorted(ElementsAre(1, 2, 3)));
}

TEST_F(ScriptRunnerTest, LateNotifications) {
  auto* pending_script1 = MockPendingScript::CreateInOrder(document_);
  auto* pending_script2 = MockPendingScript::CreateInOrder(document_);

  QueueScriptForExecution(pending_script1);
  QueueScriptForExecution(pending_script2);

  EXPECT_CALL(*pending_script1, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(1); }));
  EXPECT_CALL(*pending_script2, ExecuteScriptBlock())
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(2); }));

  NotifyScriptReady(pending_script1);
  platform_->RunUntilIdle();

  // At this moment all tasks can be already executed. Make sure that we do not
  // crash here.
  NotifyScriptReady(pending_script2);
  platform_->RunUntilIdle();

  EXPECT_THAT(order_, ElementsAre(1, 2));
}

TEST_F(ScriptRunnerTest, TasksWithDeadScriptRunner) {
  Persistent<MockPendingScript> pending_script1 =
      MockPendingScript::CreateAsync(document_);
  Persistent<MockPendingScript> pending_script2 =
      MockPendingScript::CreateAsync(document_);

  QueueScriptForExecution(pending_script1);
  QueueScriptForExecution(pending_script2);

  NotifyScriptReady(pending_script1);
  NotifyScriptReady(pending_script2);

  script_runner_.Release();

  ThreadState::Current()->CollectAllGarbageForTesting();

  // m_scriptRunner is gone. We need to make sure that ScriptRunner::Task do not
  // access dead object.
  EXPECT_CALL(*pending_script1, ExecuteScriptBlock()).Times(0);
  EXPECT_CALL(*pending_script2, ExecuteScriptBlock()).Times(0);

  platform_->RunUntilIdle();
}

TEST_F(ScriptRunnerTest, TryStreamWhenEnqueingScript) {
  auto* pending_script1 = MockPendingScript::CreateAsync(document_);
  pending_script1->SetIsReady(true);
  QueueScriptForExecution(pending_script1);
}

TEST_F(ScriptRunnerTest, DelayReasons) {
  // Script waiting only for loading.
  MockPendingScript* pending_script1 =
      MockPendingScript::CreateAsync(document_);

  // Script waiting for one additional delay reason.
  MockPendingScript* pending_script2 =
      MockPendingScript::CreateAsync(document_);

  // Script waiting for two additional delay reason.
  MockPendingScript* pending_script3 =
      MockPendingScript::CreateAsync(document_);

  // Script waiting for an additional delay reason that is removed before load
  // completion.
  MockPendingScript* pending_script4 =
      MockPendingScript::CreateAsync(document_);

  using Checkpoint = testing::StrictMock<testing::MockFunction<void(int)>>;
  Checkpoint checkpoint;
  ::testing::InSequence s;

  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*pending_script1, ExecuteScriptBlock());
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*pending_script2, ExecuteScriptBlock());
  EXPECT_CALL(checkpoint, Call(4));
  EXPECT_CALL(checkpoint, Call(5));
  EXPECT_CALL(*pending_script3, ExecuteScriptBlock());
  EXPECT_CALL(checkpoint, Call(6));
  EXPECT_CALL(checkpoint, Call(7));
  EXPECT_CALL(*pending_script4, ExecuteScriptBlock());
  EXPECT_CALL(checkpoint, Call(8));
  EXPECT_CALL(checkpoint, Call(9));

  auto* delayer1 = MakeGarbageCollected<ScriptRunnerDelayer>(
      script_runner_, ScriptRunner::DelayReason::kTest1);
  auto* delayer2 = MakeGarbageCollected<ScriptRunnerDelayer>(
      script_runner_, ScriptRunner::DelayReason::kTest2);
  delayer1->Activate();
  delayer1->Activate();
  delayer2->Activate();

  script_runner_->QueueScriptForExecution(
      pending_script1, static_cast<int>(ScriptRunner::DelayReason::kLoad));
  script_runner_->QueueScriptForExecution(
      pending_script2, static_cast<int>(ScriptRunner::DelayReason::kLoad) |
                           static_cast<int>(ScriptRunner::DelayReason::kTest1));
  script_runner_->QueueScriptForExecution(
      pending_script3, static_cast<int>(ScriptRunner::DelayReason::kLoad) |
                           static_cast<int>(ScriptRunner::DelayReason::kTest1) |
                           static_cast<int>(ScriptRunner::DelayReason::kTest2));
  script_runner_->QueueScriptForExecution(
      pending_script4, static_cast<int>(ScriptRunner::DelayReason::kLoad) |
                           static_cast<int>(ScriptRunner::DelayReason::kTest1));

  NotifyScriptReady(pending_script1);
  NotifyScriptReady(pending_script2);
  NotifyScriptReady(pending_script3);

  checkpoint.Call(1);
  platform_->RunUntilIdle();
  checkpoint.Call(2);
  delayer1->Deactivate();
  checkpoint.Call(3);
  platform_->RunUntilIdle();

  checkpoint.Call(4);
  delayer2->Deactivate();
  checkpoint.Call(5);
  platform_->RunUntilIdle();

  checkpoint.Call(6);
  NotifyScriptReady(pending_script4);
  checkpoint.Call(7);
  platform_->RunUntilIdle();

  checkpoint.Call(8);
  delayer2->Deactivate();
  checkpoint.Call(9);
  platform_->RunUntilIdle();
}

class PostTaskWithLowPriorityUntilTimeoutTest : public testing::Test {
 public:
  PostTaskWithLowPriorityUntilTimeoutTest()
      : task_runner_(platform_->test_task_runner()),
        null_task_runner_(base::MakeRefCounted<base::NullTaskRunner>()) {}

 protected:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  scoped_refptr<base::NullTaskRunner> null_task_runner_;
};

TEST_F(PostTaskWithLowPriorityUntilTimeoutTest, RunTaskOnce) {
  int counter = 0;
  base::OnceClosure task = WTF::BindOnce([](int* counter) { (*counter)++; },
                                         WTF::Unretained(&counter));

  PostTaskWithLowPriorityUntilTimeoutForTesting(
      FROM_HERE, std::move(task), base::Seconds(1),
      /*lower_priority_task_runner=*/task_runner_,
      /*normal_priority_task_runner=*/task_runner_);

  EXPECT_EQ(0, counter);
  EXPECT_EQ(2u, task_runner_->GetPendingTaskCount());
  platform_->SetAutoAdvanceNowToPendingTasks(true);
  platform_->RunUntilIdle();
  EXPECT_EQ(1, counter);
  EXPECT_EQ(0u, task_runner_->GetPendingTaskCount());
}

TEST_F(PostTaskWithLowPriorityUntilTimeoutTest, RunOnLowerPriorityTaskRunner) {
  int counter = 0;
  base::OnceClosure task = WTF::BindOnce([](int* counter) { (*counter)++; },
                                         WTF::Unretained(&counter));

  PostTaskWithLowPriorityUntilTimeoutForTesting(
      FROM_HERE, std::move(task), base::Seconds(1),
      /*lower_priority_task_runner=*/task_runner_,
      /*normal_priority_task_runner=*/null_task_runner_);

  EXPECT_EQ(0, counter);
  EXPECT_EQ(1u, task_runner_->GetPendingTaskCount());
  platform_->RunSingleTask();
  EXPECT_EQ(1, counter);
  EXPECT_EQ(0u, task_runner_->GetPendingTaskCount());
}

TEST_F(PostTaskWithLowPriorityUntilTimeoutTest, RunOnNormalPriorityTaskRunner) {
  int counter = 0;
  base::OnceClosure task = WTF::BindOnce([](int* counter) { (*counter)++; },
                                         WTF::Unretained(&counter));

  PostTaskWithLowPriorityUntilTimeoutForTesting(
      FROM_HERE, std::move(task), base::Seconds(1),
      /*lower_priority_task_runner=*/null_task_runner_,
      /*normal_priority_task_runner=*/task_runner_);

  EXPECT_EQ(0, counter);
  EXPECT_EQ(1u, task_runner_->GetPendingTaskCount());
  platform_->RunSingleTask();
  EXPECT_EQ(1, counter);
  EXPECT_EQ(0u, task_runner_->GetPendingTaskCount());
}

}  // namespace blink
