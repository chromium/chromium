// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/worklet/animation_and_paint_worklet_thread.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/script/js_module_script.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/testing/module_test_base.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/parent_execution_context_task_runners.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/core/workers/worklet_thread_holder.h"
#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_proxy_client.h"
#include "third_party/blink/renderer/modules/worklet/worklet_thread_test_common.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"

namespace blink {
namespace {

class TestAnimationWorkletProxyClient : public AnimationWorkletProxyClient {
 public:
  TestAnimationWorkletProxyClient()
      : AnimationWorkletProxyClient(0, nullptr, nullptr, nullptr, nullptr) {}
  void AddGlobalScope(WorkletGlobalScope*) override {}
};

}  // namespace

class AnimationAndPaintWorkletThreadTest : public PageTestBase,
                                           public ModuleTestBase {
 public:
  void SetUp() override {
    ModuleTestBase::SetUp();
    PageTestBase::SetUp(gfx::Size());
    NavigateTo(KURL("https://example.com/"));
    reporting_proxy_ = std::make_unique<WorkerReportingProxy>();
  }

  void TearDown() override {
    PageTestBase::TearDown();
    ModuleTestBase::TearDown();
  }

  // Attempts to run some simple script for |thread|.
  void CheckWorkletCanExecuteScript(WorkerThread* thread) {
    std::unique_ptr<base::WaitableEvent> wait_event =
        std::make_unique<base::WaitableEvent>();
    PostCrossThreadTask(
        *thread->GetWorkerBackingThread().BackingThread().GetTaskRunner(),
        FROM_HERE,
        CrossThreadBindOnce(
            &AnimationAndPaintWorkletThreadTest::ExecuteScriptInWorklet,
            CrossThreadUnretained(this), CrossThreadUnretained(thread),
            CrossThreadUnretained(wait_event.get())));
    wait_event->Wait();
  }

  std::unique_ptr<WorkerReportingProxy> reporting_proxy_;

 private:
  void ExecuteScriptInWorklet(WorkerThread* thread,
                              base::WaitableEvent* wait_event) {
    ScriptState* script_state =
        thread->GlobalScope()->ScriptController()->GetScriptState();
    EXPECT_TRUE(script_state);
    ScriptState::Scope scope(script_state);
    const KURL js_url("https://example.com/foo.js");
    v8::Local<v8::Module> module = ModuleTestBase::CompileModule(
        script_state, "var counter = 0; ++counter;", js_url);
    EXPECT_FALSE(module.IsEmpty());
    ScriptValue exception =
        ModuleRecord::Instantiate(script_state, module, js_url);
    EXPECT_TRUE(exception.IsEmpty());
    ScriptEvaluationResult result =
        JSModuleScript::CreateForTest(Modulator::From(script_state), module,
                                      js_url)
            ->RunScriptOnScriptStateAndReturnValue(script_state);
    EXPECT_FALSE(GetResult(script_state, std::move(result)).IsEmpty());
    wait_event->Signal();
  }
};

TEST_F(AnimationAndPaintWorkletThreadTest, Basic) {
  std::unique_ptr<AnimationAndPaintWorkletThread> worklet =
      CreateThreadAndProvideAnimationWorkletProxyClient(&GetDocument(),
                                                        reporting_proxy_.get());
  CheckWorkletCanExecuteScript(worklet.get());
  worklet->Terminate();
  worklet->WaitForShutdownForTesting();
}

// Tests that the same WebThread is used for new worklets if the WebThread is
// still alive.
TEST_F(AnimationAndPaintWorkletThreadTest, CreateSecondAndTerminateFirst) {
  // Create the first worklet and wait until it is initialized.
  std::unique_ptr<AnimationAndPaintWorkletThread> first_worklet =
      CreateThreadAndProvideAnimationWorkletProxyClient(&GetDocument(),
                                                        reporting_proxy_.get());
  Thread* first_thread =
      &first_worklet->GetWorkerBackingThread().BackingThread();
  CheckWorkletCanExecuteScript(first_worklet.get());
  v8::Isolate* first_isolate = first_worklet->GetIsolate();
  ASSERT_TRUE(first_isolate);

  // Create the second worklet and immediately destroy the first worklet.
  std::unique_ptr<AnimationAndPaintWorkletThread> second_worklet =
      CreateThreadAndProvideAnimationWorkletProxyClient(&GetDocument(),
                                                        reporting_proxy_.get());
  // We don't use terminateAndWait here to avoid forcible termination.
  first_worklet->Terminate();
  first_worklet->WaitForShutdownForTesting();

  // Wait until the second worklet is initialized. Verify that the second
  // worklet is using the same thread and Isolate as the first worklet.
  Thread* second_thread =
      &second_worklet->GetWorkerBackingThread().BackingThread();
  ASSERT_EQ(first_thread, second_thread);

  v8::Isolate* second_isolate = second_worklet->GetIsolate();
  ASSERT_TRUE(second_isolate);
  EXPECT_EQ(first_isolate, second_isolate);

  // Verify that the worklet can still successfully execute script.
  CheckWorkletCanExecuteScript(second_worklet.get());

  second_worklet->Terminate();
  second_worklet->WaitForShutdownForTesting();
}

// Tests that the WebThread is reused if all existing worklets are terminated
// before a new worklet is created, as long as the worklets are not destructed.
TEST_F(AnimationAndPaintWorkletThreadTest, TerminateFirstAndCreateSecond) {
  // Create the first worklet, wait until it is initialized, and terminate it.
  std::unique_ptr<AnimationAndPaintWorkletThread> worklet =
      CreateThreadAndProvideAnimationWorkletProxyClient(&GetDocument(),
                                                        reporting_proxy_.get());
  Thread* first_thread = &worklet->GetWorkerBackingThread().BackingThread();
  CheckWorkletCanExecuteScript(worklet.get());

  // We don't use terminateAndWait here to avoid forcible termination.
  worklet->Terminate();
  worklet->WaitForShutdownForTesting();

  // Create the second worklet. The backing thread is same.
  worklet = CreateThreadAndProvideAnimationWorkletProxyClient(
      &GetDocument(), reporting_proxy_.get());
  Thread* second_thread = &worklet->GetWorkerBackingThread().BackingThread();
  EXPECT_EQ(first_thread, second_thread);
  CheckWorkletCanExecuteScript(worklet.get());

  worklet->Terminate();
  worklet->WaitForShutdownForTesting();
}

// Tests that v8::Isolate and WebThread are correctly set-up if a worklet is
// created while another is terminating.
TEST_F(AnimationAndPaintWorkletThreadTest,
       CreatingSecondDuringTerminationOfFirst) {
  std::unique_ptr<AnimationAndPaintWorkletThread> first_worklet =
      CreateThreadAndProvideAnimationWorkletProxyClient(&GetDocument(),
                                                        reporting_proxy_.get());
  CheckWorkletCanExecuteScript(first_worklet.get());
  v8::Isolate* first_isolate = first_worklet->GetIsolate();
  ASSERT_TRUE(first_isolate);

  // Request termination of the first worklet and create the second worklet
  // as soon as possible.
  first_worklet->Terminate();
  // We don't wait for its termination.
  // Note: We rely on the assumption that the termination steps don't run
  // on the worklet thread so quickly. This could be a source of flakiness.

  std::unique_ptr<AnimationAndPaintWorkletThread> second_worklet =
      CreateThreadAndProvideAnimationWorkletProxyClient(&GetDocument(),
                                                        reporting_proxy_.get());

  v8::Isolate* second_isolate = second_worklet->GetIsolate();
  ASSERT_TRUE(second_isolate);
  EXPECT_EQ(first_isolate, second_isolate);

  // Verify that the isolate can run some scripts correctly in the second
  // worklet.
  CheckWorkletCanExecuteScript(second_worklet.get());
  second_worklet->Terminate();
  second_worklet->WaitForShutdownForTesting();
}

// Tests that the backing thread is correctly created, torn down, and recreated
// as AnimationWorkletThreads are created and destroyed.
TEST_F(AnimationAndPaintWorkletThreadTest,
       WorkletThreadHolderIsRefCountedProperly) {
  EXPECT_FALSE(
      AnimationAndPaintWorkletThread::GetWorkletThreadHolderForTesting());

  std::unique_ptr<AnimationAndPaintWorkletThread> worklet =
      CreateThreadAndProvideAnimationWorkletProxyClient(&GetDocument(),
                                                        reporting_proxy_.get());
  ASSERT_TRUE(worklet.get());
  WorkletThreadHolder<AnimationAndPaintWorkletThread>* holder =
      AnimationAndPaintWorkletThread::GetWorkletThreadHolderForTesting();
  EXPECT_TRUE(holder);

  std::unique_ptr<AnimationAndPaintWorkletThread> worklet2 =
      CreateThreadAndProvideAnimationWorkletProxyClient(&GetDocument(),
                                                        reporting_proxy_.get());
  ASSERT_TRUE(worklet2.get());
  WorkletThreadHolder<AnimationAndPaintWorkletThread>* holder2 =
      AnimationAndPaintWorkletThread::GetWorkletThreadHolderForTesting();
  EXPECT_EQ(holder, holder2);

  worklet->Terminate();
  worklet->WaitForShutdownForTesting();
  worklet.reset();
  EXPECT_TRUE(
      AnimationAndPaintWorkletThread::GetWorkletThreadHolderForTesting());

  worklet2->Terminate();
  worklet2->WaitForShutdownForTesting();
  worklet2.reset();
  EXPECT_FALSE(
      AnimationAndPaintWorkletThread::GetWorkletThreadHolderForTesting());

  std::unique_ptr<AnimationAndPaintWorkletThread> worklet3 =
      CreateThreadAndProvideAnimationWorkletProxyClient(&GetDocument(),
                                                        reporting_proxy_.get());
  ASSERT_TRUE(worklet3.get());
  EXPECT_TRUE(
      AnimationAndPaintWorkletThread::GetWorkletThreadHolderForTesting());

  worklet3->Terminate();
  worklet3->WaitForShutdownForTesting();
}

}  // namespace blink
