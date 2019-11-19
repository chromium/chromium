// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/script_runner.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/script/mock_script_element_base.h"
#include "third_party/blink/renderer/core/script/pending_script.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
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
      : PendingScript(element, TextPosition()) {
    SetSchedulingType(scheduling_type);
  }
  ~MockPendingScript() override {}

  MOCK_CONST_METHOD0(GetScriptType, mojom::ScriptType());
  MOCK_CONST_METHOD1(CheckMIMETypeBeforeRunScript, bool(Document*));
  MOCK_CONST_METHOD1(GetSource, Script*(const KURL&));
  MOCK_CONST_METHOD0(IsExternal, bool());
  MOCK_CONST_METHOD0(WasCanceled, bool());
  MOCK_CONST_METHOD0(UrlForTracing, KURL());
  MOCK_METHOD0(RemoveFromMemoryCache, void());
  MOCK_METHOD1(ExecuteScriptBlock, void(const KURL&));

  void StartStreamingIfPossible() override {}

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
  ScriptRunnerTest() : document_(MakeGarbageCollected<Document>()) {}

  void SetUp() override {
    // We have to create ScriptRunner after initializing platform, because we
    // need Platform::current()->currentThread()->scheduler()->
    // loadingTaskRunner() to be initialized before creating ScriptRunner to
    // save it in constructor.
    script_runner_ = MakeGarbageCollected<ScriptRunner>(document_.Get());
    RuntimeCallStats::SetRuntimeCallStatsForTesting();
  }
  void TearDown() override {
    script_runner_.Release();
    RuntimeCallStats::ClearRuntimeCallStatsForTesting();
  }

 protected:
  void NotifyScriptReady(MockPendingScript* pending_script) {
    pending_script->SetIsReady(true);
    script_runner_->NotifyScriptReady(pending_script);
  }

  void QueueScriptForExecution(MockPendingScript* pending_script) {
    script_runner_->QueueScriptForExecution(pending_script);
  }

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

  EXPECT_CALL(*pending_script, ExecuteScriptBlock(_));
  platform_->RunUntilIdle();
}

TEST_F(ScriptRunnerTest, QueueSingleScript_InOrder) {
  auto* pending_script = MockPendingScript::CreateInOrder(document_);
  QueueScriptForExecution(pending_script);

  EXPECT_CALL(*pending_script, ExecuteScriptBlock(_));

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
    EXPECT_CALL(*pending_scripts[i], ExecuteScriptBlock(_))
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
  NotifyScriptReady(pending_script2);
  NotifyScriptReady(pending_script3);
  NotifyScriptReady(pending_script4);
  NotifyScriptReady(pending_script5);

  EXPECT_CALL(*pending_script1, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(1); }));
  EXPECT_CALL(*pending_script2, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(2); }));
  EXPECT_CALL(*pending_script3, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(3); }));
  EXPECT_CALL(*pending_script4, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(4); }));
  EXPECT_CALL(*pending_script5, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(5); }));

  platform_->RunUntilIdle();

  // Async tasks are expected to run first.
  EXPECT_THAT(order_, ElementsAre(4, 5, 1, 2, 3));
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
  EXPECT_CALL(*pending_script1, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([pending_script, this] {
        order_.push_back(1);
        NotifyScriptReady(pending_script);
      }));

  pending_script = pending_script3;
  EXPECT_CALL(*pending_script2, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([pending_script, this] {
        order_.push_back(2);
        NotifyScriptReady(pending_script);
      }));

  EXPECT_CALL(*pending_script3, ExecuteScriptBlock(_))
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
  EXPECT_CALL(*pending_script1, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([pending_script, &pending_script2, this] {
        order_.push_back(1);
        QueueScriptForExecution(pending_script);
        NotifyScriptReady(pending_script2);
      }));

  pending_script = pending_script3;
  EXPECT_CALL(*pending_script2, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([pending_script, &pending_script3, this] {
        order_.push_back(2);
        QueueScriptForExecution(pending_script);
        NotifyScriptReady(pending_script3);
      }));

  EXPECT_CALL(*pending_script3, ExecuteScriptBlock(_))
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
      EXPECT_CALL(*pending_scripts[i], ExecuteScriptBlock(_))
          .WillOnce(InvokeWithoutArgs([this, i] { order_.push_back(i); }));
    }
  }

  NotifyScriptReady(pending_scripts[0]);
  NotifyScriptReady(pending_scripts[1]);

  EXPECT_CALL(*pending_scripts[0], ExecuteScriptBlock(_))
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

  EXPECT_CALL(*pending_script1, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(1); }));
  EXPECT_CALL(*pending_script2, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(2); }));
  EXPECT_CALL(*pending_script3, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(3); }));

  NotifyScriptReady(pending_script1);
  NotifyScriptReady(pending_script2);
  NotifyScriptReady(pending_script3);

  platform_->RunSingleTask();
  script_runner_->Suspend();
  script_runner_->Resume();
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

  EXPECT_CALL(*pending_script1, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(1); }));
  EXPECT_CALL(*pending_script2, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(2); }));
  EXPECT_CALL(*pending_script3, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(3); }));

  platform_->RunSingleTask();
  script_runner_->Suspend();
  script_runner_->Resume();
  platform_->RunUntilIdle();

  // Make sure elements are correct.
  EXPECT_THAT(order_, WhenSorted(ElementsAre(1, 2, 3)));
}

TEST_F(ScriptRunnerTest, SetForceDeferredWithAddedAsyncScript) {
  auto* pending_script1 = MockPendingScript::CreateAsync(document_);

  QueueScriptForExecution(pending_script1);
  NotifyScriptReady(pending_script1);
  EXPECT_CALL(*pending_script1, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(1); }));
  script_runner_->SetForceDeferredExecution(true);

  // Adding new async script while deferred will cause another task to be
  // posted for it when execution is unblocked.
  auto* pending_script2 = MockPendingScript::CreateAsync(document_);
  QueueScriptForExecution(pending_script2);
  NotifyScriptReady(pending_script2);
  EXPECT_CALL(*pending_script2, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(2); }));
  // Unblock async scripts before the tasks posted in NotifyScriptReady() is
  // executed, i.e. no RunUntilIdle() etc. in between.
  script_runner_->SetForceDeferredExecution(false);
  platform_->RunUntilIdle();
  ASSERT_EQ(2u, order_.size());
}

TEST_F(ScriptRunnerTest, SetForceDeferredAndResumeAndSuspend) {
  auto* pending_script1 = MockPendingScript::CreateAsync(document_);

  QueueScriptForExecution(pending_script1);
  NotifyScriptReady(pending_script1);

  EXPECT_CALL(*pending_script1, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(1); }));

  script_runner_->SetForceDeferredExecution(true);
  platform_->RunSingleTask();
  ASSERT_EQ(0u, order_.size());

  script_runner_->Suspend();
  platform_->RunSingleTask();
  ASSERT_EQ(0u, order_.size());

  // Resume will not execute script while still in ForceDeferred state.
  script_runner_->Resume();
  platform_->RunUntilIdle();
  ASSERT_EQ(0u, order_.size());

  script_runner_->SetForceDeferredExecution(false);
  platform_->RunUntilIdle();
  ASSERT_EQ(1u, order_.size());
}

TEST_F(ScriptRunnerTest, LateNotifications) {
  auto* pending_script1 = MockPendingScript::CreateInOrder(document_);
  auto* pending_script2 = MockPendingScript::CreateInOrder(document_);

  QueueScriptForExecution(pending_script1);
  QueueScriptForExecution(pending_script2);

  EXPECT_CALL(*pending_script1, ExecuteScriptBlock(_))
      .WillOnce(InvokeWithoutArgs([this] { order_.push_back(1); }));
  EXPECT_CALL(*pending_script2, ExecuteScriptBlock(_))
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
  EXPECT_CALL(*pending_script1, ExecuteScriptBlock(_)).Times(0);
  EXPECT_CALL(*pending_script2, ExecuteScriptBlock(_)).Times(0);

  platform_->RunUntilIdle();
}

TEST_F(ScriptRunnerTest, TryStreamWhenEnqueingScript) {
  auto* pending_script1 = MockPendingScript::CreateAsync(document_);
  pending_script1->SetIsReady(true);
  QueueScriptForExecution(pending_script1);
}

}  // namespace blink
