// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/font_face_set_worker.h"

#include "base/run_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_font_face_descriptors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_string.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

using ::testing::_;

void RunFontFaceSetOperationsAfterFreezeAndTerminate(
    WorkerThread* worker_thread,
    CrossThreadOnceClosure quit_closure) {
  ASSERT_TRUE(worker_thread->IsCurrentThread());
  auto* scope = To<WorkerGlobalScope>(worker_thread->GlobalScope());
  ScriptState* script_state = scope->ScriptController()->GetScriptState();
  FontFaceSet* fonts = scope->fonts();

  FontFace* face1 = nullptr;
  FontFace* face2 = nullptr;
  {
    ScriptState::Scope script_scope(script_state);
    auto* source =
        MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrString>(
            String("local(Arial)"));
    auto* descriptors = FontFaceDescriptors::Create();
    face1 =
        FontFace::Create(scope, AtomicString("TestFont1"), source, descriptors);
    face2 =
        FontFace::Create(scope, AtomicString("TestFont2"), source, descriptors);
    fonts->addForBinding(script_state, face1, ASSERT_NO_EXCEPTION);
    EXPECT_EQ(1u, fonts->size());
    EXPECT_TRUE(fonts->hasForBinding(script_state, face1, ASSERT_NO_EXCEPTION));
  }

  // Ask the main thread to terminate this worker while it is frozen in a
  // nested message loop. `PrepareForShutdownOnWorkerThread()` runs inside the
  // nested loop, destroys the execution context, and quits the nested loop
  // before `PerformShutdownOnWorkerThread()` disposes the global scope.
  PostCrossThreadTask(
      *worker_thread->GetParentTaskRunnerForTesting(), FROM_HERE,
      CrossThreadBindOnce(&WorkerThread::Terminate,
                          CrossThreadUnretained(worker_thread)));
  worker_thread->Freeze(/*is_in_back_forward_cache=*/false);

  EXPECT_TRUE(scope->ExecutionContext::IsContextDestroyed());
  EXPECT_EQ(nullptr, fonts->GetExecutionContext());

  {
    ScriptState::Scope script_scope(script_state);
    EXPECT_FALSE(
        fonts->deleteForBinding(script_state, face1, ASSERT_NO_EXCEPTION));
    EXPECT_FALSE(
        fonts->hasForBinding(script_state, face1, ASSERT_NO_EXCEPTION));
    EXPECT_EQ(1u, fonts->size());
    EXPECT_EQ(fonts,
              fonts->addForBinding(script_state, face2, ASSERT_NO_EXCEPTION));
    fonts->clearForBinding(script_state, ASSERT_NO_EXCEPTION);
    EXPECT_FALSE(fonts->check("10px TestFont1", "a", ASSERT_NO_EXCEPTION));
    ScriptPromise<IDLSequence<FontFace>> load_promise =
        fonts->load(script_state, "10px TestFont1", "a");
    EXPECT_TRUE(load_promise.IsEmpty());
    EXPECT_NE(nullptr, fonts->valuesForBinding(script_state));
    ScriptPromise<FontFaceSet> ready_promise = fonts->ready(script_state);
    EXPECT_TRUE(ready_promise.IsEmpty());
  }

  PostCrossThreadTask(*worker_thread->GetParentTaskRunnerForTesting(),
                      FROM_HERE, std::move(quit_closure));
}

}  // namespace

// Regression test for https://crbug.com/576337357.
TEST(FontFaceSetWorkerTest, OperationsAfterContextDestroyedInNestedLoop) {
  test::TaskEnvironment task_environment;
  MockWorkerReportingProxy reporting_proxy;
  EXPECT_CALL(reporting_proxy, DidCreateWorkerGlobalScope(_)).Times(1);
  EXPECT_CALL(reporting_proxy, WillEvaluateScriptMock()).Times(1);
  EXPECT_CALL(reporting_proxy, DidEvaluateTopLevelScriptMock(true, _)).Times(1);
  EXPECT_CALL(reporting_proxy, WillDestroyWorkerGlobalScope()).Times(1);
  EXPECT_CALL(reporting_proxy, DidTerminateWorkerThread()).Times(1);

  WorkerThreadForTest worker_thread(reporting_proxy);
  scoped_refptr<const SecurityOrigin> security_origin =
      SecurityOrigin::Create(KURL("http://fake.url/"));
  worker_thread.StartWithSourceCode(security_origin.get(),
                                    "// fake source code");
  reporting_proxy.WaitUntilDidEvaluateTopLevelScript();

  base::RunLoop run_loop;
  PostCrossThreadTask(
      *worker_thread.GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&RunFontFaceSetOperationsAfterFreezeAndTerminate,
                          CrossThreadUnretained(&worker_thread),
                          CrossThreadBindOnce(run_loop.QuitClosure())));
  run_loop.Run();

  worker_thread.WaitForShutdownForTesting();
}

}  // namespace blink
