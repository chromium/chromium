// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_worklet_global_scope.h"

#include "base/synchronization/waitable_event.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/modules/csspaint/css_paint_definition.h"
#include "third_party/blink/renderer/modules/csspaint/document_paint_definition.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_proxy_client.h"
#include "third_party/blink/renderer/modules/worklet/animation_and_paint_worklet_thread.h"
#include "third_party/blink/renderer/modules/worklet/worklet_thread_test_common.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

// TODO(smcgruer): Extract a common base class between this and
// AnimationWorkletGlobalScope.
class PaintWorkletGlobalScopeTest : public PageTestBase {
 public:
  PaintWorkletGlobalScopeTest() = default;

  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());
    NavigateTo(KURL("https://example.com/"));
    // This test only needs the proxy client set to avoid calling
    // PaintWorkletProxyClient::Create, but it doesn't need the dispatcher/etc.
    proxy_client_ = MakeGarbageCollected<PaintWorkletProxyClient>(
        1, nullptr, GetFrame().GetTaskRunner(TaskType::kInternalDefault),
        nullptr, nullptr);
    reporting_proxy_ = std::make_unique<WorkerReportingProxy>();
  }

  using TestCallback =
      void (PaintWorkletGlobalScopeTest::*)(WorkerThread*,
                                            PaintWorkletProxyClient*,
                                            base::WaitableEvent*);

  // Create a new paint worklet and run the callback task on it. Terminate the
  // worklet once the task completion is signaled.
  void RunTestOnWorkletThread(TestCallback callback) {
    std::unique_ptr<WorkerThread> worklet =
        CreateThreadAndProvidePaintWorkletProxyClient(
            &GetDocument(), reporting_proxy_.get(), proxy_client_);
    base::WaitableEvent waitable_event;
    PostCrossThreadTask(
        *worklet->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(
            callback, CrossThreadUnretained(this),
            CrossThreadUnretained(worklet.get()),
            CrossThreadPersistent<PaintWorkletProxyClient>(proxy_client_),
            CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();
    waitable_event.Reset();

    worklet->Terminate();
    worklet->WaitForShutdownForTesting();
  }

  void RunBasicParsingTestOnWorklet(WorkerThread* thread,
                                    PaintWorkletProxyClient* proxy_client,
                                    base::WaitableEvent* waitable_event) {
    ASSERT_TRUE(thread->IsCurrentThread());
    CrossThreadPersistent<PaintWorkletGlobalScope> global_scope =
        WrapCrossThreadPersistent(
            To<PaintWorkletGlobalScope>(thread->GlobalScope()));

    {
      // registerPaint() with a valid class definition should define a painter.
      String source_code =
          R"JS(
            registerPaint('test', class {
              constructor () {}
              paint (ctx, size) {}
            });
          )JS";
      ClassicScript::CreateUnspecifiedScript(source_code)
          ->RunScriptOnScriptState(
              global_scope->ScriptController()->GetScriptState());
      CSSPaintDefinition* definition = global_scope->FindDefinition("test");
      ASSERT_TRUE(definition);
    }

    {
      // registerPaint() with a null class definition should fail to define a
      // painter.
      String source_code = "registerPaint('null', null);";
      ClassicScript::CreateUnspecifiedScript(source_code)
          ->RunScriptOnScriptState(
              global_scope->ScriptController()->GetScriptState());
      EXPECT_FALSE(global_scope->FindDefinition("null"));
    }

    EXPECT_FALSE(global_scope->FindDefinition("non-existent"));

    waitable_event->Signal();
  }

 private:
  Persistent<PaintWorkletProxyClient> proxy_client_;
  std::unique_ptr<WorkerReportingProxy> reporting_proxy_;
};

TEST_F(PaintWorkletGlobalScopeTest, BasicParsing) {
  ScopedOffMainThreadCSSPaintForTest off_main_thread_css_paint(true);
  RunTestOnWorkletThread(
      &PaintWorkletGlobalScopeTest::RunBasicParsingTestOnWorklet);
}

}  // namespace blink
