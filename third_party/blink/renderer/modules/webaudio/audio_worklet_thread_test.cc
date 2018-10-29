// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_thread.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_module.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cache_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_inspector_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/access_control_status.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/waitable_event.h"
#include "third_party/blink/renderer/platform/web_thread_supporting_gc.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"

namespace blink {

class AudioWorkletThreadTest : public PageTestBase {
 public:
  void SetUp() override {
    AudioWorkletThread::EnsureSharedBackingThread();
    PageTestBase::SetUp(IntSize());
    Document* document = &GetDocument();
    document->SetURL(KURL("https://example.com/"));
    document->UpdateSecurityOrigin(SecurityOrigin::Create(document->Url()));
    reporting_proxy_ = std::make_unique<WorkerReportingProxy>();
  }

  std::unique_ptr<AudioWorkletThread> CreateAudioWorkletThread() {
    std::unique_ptr<AudioWorkletThread> thread =
        AudioWorkletThread::Create(*reporting_proxy_);
    Document* document = &GetDocument();
    thread->Start(
        std::make_unique<GlobalScopeCreationParams>(
            document->Url(), ScriptType::kModule, document->UserAgent(),
            Vector<CSPHeaderAndType>(), document->GetReferrerPolicy(),
            document->GetSecurityOrigin(), document->IsSecureContext(),
            document->GetHttpsState(), nullptr /* worker_clients */,
            document->AddressSpace(),
            OriginTrialContext::GetTokens(document).get(),
            base::UnguessableToken::Create(), nullptr /* worker_settings */,
            kV8CacheOptionsDefault, new WorkletModuleResponsesMap),
        base::nullopt, WorkerInspectorProxy::PauseOnWorkerStart::kDontPause,
        ParentExecutionContextTaskRunners::Create());
    return thread;
  }

  // Attempts to run some simple script for |thread|.
  void CheckWorkletCanExecuteScript(WorkerThread* thread) {
    WaitableEvent wait_event;
    thread->GetWorkerBackingThread().BackingThread().PostTask(
        FROM_HERE,
        CrossThreadBind(&AudioWorkletThreadTest::ExecuteScriptInWorklet,
                        CrossThreadUnretained(this),
                        CrossThreadUnretained(thread),
                        CrossThreadUnretained(&wait_event)));
    wait_event.Wait();
  }

 private:
  void ExecuteScriptInWorklet(WorkerThread* thread, WaitableEvent* wait_event) {
    ScriptState* script_state =
        thread->GlobalScope()->ScriptController()->GetScriptState();
    EXPECT_TRUE(script_state);
    ScriptState::Scope scope(script_state);
    KURL js_url("https://example.com/worklet.js");
    ScriptModule module = ScriptModule::Compile(
        script_state->GetIsolate(), "var counter = 0; ++counter;", js_url,
        js_url, ScriptFetchOptions(), kSharableCrossOrigin,
        TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
    EXPECT_FALSE(module.IsNull());
    ScriptValue exception = module.Instantiate(script_state);
    EXPECT_TRUE(exception.IsEmpty());
    ScriptValue value = module.Evaluate(script_state);
    EXPECT_TRUE(value.IsEmpty());
    wait_event->Signal();
  }

  std::unique_ptr<WorkerReportingProxy> reporting_proxy_;
};

TEST_F(AudioWorkletThreadTest, Basic) {
  std::unique_ptr<AudioWorkletThread> worklet = CreateAudioWorkletThread();
  CheckWorkletCanExecuteScript(worklet.get());
  worklet->Terminate();
  worklet->WaitForShutdownForTesting();
}

// Tests that the same WebThread is used for new worklets if the WebThread is
// still alive.
TEST_F(AudioWorkletThreadTest, CreateSecondAndTerminateFirst) {
  // Create the first worklet and wait until it is initialized.
  std::unique_ptr<AudioWorkletThread> first_worklet =
      CreateAudioWorkletThread();
  WebThreadSupportingGC* first_thread =
      &first_worklet->GetWorkerBackingThread().BackingThread();
  CheckWorkletCanExecuteScript(first_worklet.get());
  v8::Isolate* first_isolate = first_worklet->GetIsolate();
  ASSERT_TRUE(first_isolate);

  // Create the second worklet and immediately destroy the first worklet.
  std::unique_ptr<AudioWorkletThread> second_worklet =
      CreateAudioWorkletThread();
  // We don't use terminateAndWait here to avoid forcible termination.
  first_worklet->Terminate();
  first_worklet->WaitForShutdownForTesting();

  // Wait until the second worklet is initialized. Verify that the second
  // worklet is using the same thread and Isolate as the first worklet.
  WebThreadSupportingGC* second_thread =
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

// Tests that a new WebThread is created if all existing worklets are
// terminated before a new worklet is created.
TEST_F(AudioWorkletThreadTest, TerminateFirstAndCreateSecond) {
  // Create the first worklet, wait until it is initialized, and terminate it.
  std::unique_ptr<AudioWorkletThread> worklet = CreateAudioWorkletThread();
  WebThreadSupportingGC* first_thread =
      &worklet->GetWorkerBackingThread().BackingThread();
  CheckWorkletCanExecuteScript(worklet.get());

  // We don't use terminateAndWait here to avoid forcible termination.
  worklet->Terminate();
  worklet->WaitForShutdownForTesting();

  // Create the second worklet. The backing thread is same.
  worklet = CreateAudioWorkletThread();
  WebThreadSupportingGC* second_thread =
      &worklet->GetWorkerBackingThread().BackingThread();
  EXPECT_EQ(first_thread, second_thread);
  CheckWorkletCanExecuteScript(worklet.get());

  worklet->Terminate();
  worklet->WaitForShutdownForTesting();
}

// Tests that v8::Isolate and WebThread are correctly set-up if a worklet is
// created while another is terminating.
TEST_F(AudioWorkletThreadTest, CreatingSecondDuringTerminationOfFirst) {
  std::unique_ptr<AudioWorkletThread> first_worklet =
      CreateAudioWorkletThread();
  CheckWorkletCanExecuteScript(first_worklet.get());
  v8::Isolate* first_isolate = first_worklet->GetIsolate();
  ASSERT_TRUE(first_isolate);

  // Request termination of the first worklet and create the second worklet
  // as soon as possible. We don't wait for its termination.
  // Note: We rely on the assumption that the termination steps don't run
  // on the worklet thread so quickly. This could be a source of flakiness.
  first_worklet->Terminate();
  std::unique_ptr<AudioWorkletThread> second_worklet =
      CreateAudioWorkletThread();

  v8::Isolate* second_isolate = second_worklet->GetIsolate();
  ASSERT_TRUE(second_isolate);
  EXPECT_EQ(first_isolate, second_isolate);

  // Verify that the isolate can run some scripts correctly in the second
  // worklet.
  CheckWorkletCanExecuteScript(second_worklet.get());
  second_worklet->Terminate();
  second_worklet->WaitForShutdownForTesting();
}

}  // namespace blink
