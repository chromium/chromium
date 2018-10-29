// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_global_scope.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_module.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cache_options.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/modules/animationworklet/animation_worklet.h"
#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_proxy_client.h"
#include "third_party/blink/renderer/modules/animationworklet/animator.h"
#include "third_party/blink/renderer/modules/animationworklet/animator_definition.h"
#include "third_party/blink/renderer/modules/worklet/animation_and_paint_worklet_thread.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/loader/fetch/access_control_status.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/waitable_event.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"

#include <memory>

namespace blink {
namespace {

class MockAnimationWorkletProxyClient : public AnimationWorkletProxyClient {
 public:
  MockAnimationWorkletProxyClient()
      : AnimationWorkletProxyClient(0, nullptr, nullptr, nullptr, nullptr),
        did_set_global_scope_(false) {}
  void SetGlobalScope(WorkletGlobalScope*) override {
    did_set_global_scope_ = true;
  }
  bool did_set_global_scope() { return did_set_global_scope_; }

 private:
  bool did_set_global_scope_;
};

}  // namespace

class AnimationWorkletGlobalScopeTest : public PageTestBase {
 public:
  AnimationWorkletGlobalScopeTest() = default;

  void SetUp() override {
    PageTestBase::SetUp(IntSize());
    Document* document = &GetDocument();
    document->SetURL(KURL("https://example.com/"));
    document->UpdateSecurityOrigin(SecurityOrigin::Create(document->Url()));
    reporting_proxy_ = std::make_unique<WorkerReportingProxy>();
  }

  std::unique_ptr<AnimationAndPaintWorkletThread>
  CreateAnimationAndPaintWorkletThread(
      AnimationWorkletProxyClient* proxy_client) {
    std::unique_ptr<AnimationAndPaintWorkletThread> thread =
        AnimationAndPaintWorkletThread::CreateForAnimationWorklet(
            *reporting_proxy_);

    WorkerClients* clients = WorkerClients::Create();
    if (proxy_client)
      ProvideAnimationWorkletProxyClientTo(clients, proxy_client);

    Document* document = &GetDocument();
    thread->Start(
        std::make_unique<GlobalScopeCreationParams>(
            document->Url(), ScriptType::kModule, document->UserAgent(),
            Vector<CSPHeaderAndType>(), document->GetReferrerPolicy(),
            document->GetSecurityOrigin(), document->IsSecureContext(),
            document->GetHttpsState(), clients, document->AddressSpace(),
            OriginTrialContext::GetTokens(document).get(),
            base::UnguessableToken::Create(), nullptr /* worker_settings */,
            kV8CacheOptionsDefault, new WorkletModuleResponsesMap),
        base::nullopt, WorkerInspectorProxy::PauseOnWorkerStart::kDontPause,
        ParentExecutionContextTaskRunners::Create());
    return thread;
  }

  using TestCalback = void (AnimationWorkletGlobalScopeTest::*)(WorkerThread*,
                                                                WaitableEvent*);
  // Create a new animation worklet and run the callback task on it. Terminate
  // the worklet once the task completion is signaled.
  void RunTestOnWorkletThread(TestCalback callback) {
    std::unique_ptr<WorkerThread> worklet =
        CreateAnimationAndPaintWorkletThread(nullptr);
    WaitableEvent waitable_event;
    PostCrossThreadTask(
        *worklet->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBind(callback, CrossThreadUnretained(this),
                        CrossThreadUnretained(worklet.get()),
                        CrossThreadUnretained(&waitable_event)));
    waitable_event.Wait();

    worklet->Terminate();
    worklet->WaitForShutdownForTesting();
  }

  void RunScriptOnWorklet(String source_code,
                          WorkerThread* thread,
                          WaitableEvent* waitable_event) {
    ASSERT_TRUE(thread->IsCurrentThread());
    auto* global_scope = To<AnimationWorkletGlobalScope>(thread->GlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    ASSERT_TRUE(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();
    ASSERT_TRUE(isolate);
    ScriptState::Scope scope(script_state);
    ASSERT_TRUE(EvaluateScriptModule(global_scope, source_code));
    waitable_event->Signal();
  }

  void RunBasicParsingTestOnWorklet(WorkerThread* thread,
                                    WaitableEvent* waitable_event) {
    ASSERT_TRUE(thread->IsCurrentThread());
    auto* global_scope = To<AnimationWorkletGlobalScope>(thread->GlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    ASSERT_TRUE(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();
    ASSERT_TRUE(isolate);

    ScriptState::Scope scope(script_state);

    {
      // registerAnimator() with a valid class definition should define an
      // animator.
      String source_code =
          R"JS(
            registerAnimator('test', class {
              constructor () {}
              animate () {}
            });
          )JS";
      ASSERT_TRUE(EvaluateScriptModule(global_scope, source_code));

      AnimatorDefinition* definition =
          global_scope->FindDefinitionForTest("test");
      ASSERT_TRUE(definition);

      EXPECT_TRUE(definition->ConstructorLocal(isolate)->IsFunction());
      EXPECT_TRUE(definition->AnimateLocal(isolate)->IsFunction());
    }

    {
      // registerAnimator() with a null class definition should fail to define
      // an animator.
      String source_code = "registerAnimator('null', null);";
      ASSERT_FALSE(EvaluateScriptModule(global_scope, source_code));
      EXPECT_FALSE(global_scope->FindDefinitionForTest("null"));
    }

    EXPECT_FALSE(global_scope->FindDefinitionForTest("non-existent"));

    waitable_event->Signal();
  }

  void RunConstructAndAnimateTestOnWorklet(WorkerThread* thread,
                                           WaitableEvent* waitable_event) {
    ASSERT_TRUE(thread->IsCurrentThread());
    auto* global_scope = To<AnimationWorkletGlobalScope>(thread->GlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    ASSERT_TRUE(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();
    ASSERT_TRUE(isolate);

    ScriptState::Scope scope(script_state);

    String source_code =
        R"JS(
            // Worklet doesn't have a reference to the global object. Instead,
            // retrieve it in a tricky way.
            var global_object = Function('return this')();
            global_object.constructed = false;
            global_object.animated = false;

            registerAnimator('test', class {
              constructor () {
                constructed = true;
              }
              animate () {
                animated = true;
              }
            });
        )JS";
    ASSERT_TRUE(EvaluateScriptModule(global_scope, source_code));

    ScriptValue constructed_before =
        global_scope->ScriptController()->EvaluateAndReturnValueForTest(
            ScriptSourceCode("Function('return this')().constructed"));
    EXPECT_FALSE(
        ToBoolean(isolate, constructed_before.V8Value(), ASSERT_NO_EXCEPTION))
        << "constructor is not invoked";

    ScriptValue animated_before =
        global_scope->ScriptController()->EvaluateAndReturnValueForTest(
            ScriptSourceCode("Function('return this')().animated"));
    EXPECT_FALSE(
        ToBoolean(isolate, animated_before.V8Value(), ASSERT_NO_EXCEPTION))
        << "animate function is invoked early";

    // Passing a new input state with a new animation id should cause the
    // worklet to create and animate an animator.
    cc::WorkletAnimationId animation_id = {1, 1};
    AnimationWorkletInput state;
    state.added_and_updated_animations.emplace_back(animation_id, "test", 5000,
                                                    nullptr, 1);

    std::unique_ptr<AnimationWorkletOutput> output =
        global_scope->Mutate(state);
    EXPECT_TRUE(output);

    ScriptValue constructed_after =
        global_scope->ScriptController()->EvaluateAndReturnValueForTest(
            ScriptSourceCode("Function('return this')().constructed"));
    EXPECT_TRUE(
        ToBoolean(isolate, constructed_after.V8Value(), ASSERT_NO_EXCEPTION))
        << "constructor is not invoked";

    ScriptValue animated_after =
        global_scope->ScriptController()->EvaluateAndReturnValueForTest(
            ScriptSourceCode("Function('return this')().animated"));
    EXPECT_TRUE(
        ToBoolean(isolate, animated_after.V8Value(), ASSERT_NO_EXCEPTION))
        << "animate function is not invoked";

    waitable_event->Signal();
  }

  void RunAnimateOutputTestOnWorklet(WorkerThread* thread,
                                     WaitableEvent* waitable_event) {
    AnimationWorkletGlobalScope* global_scope =
        static_cast<AnimationWorkletGlobalScope*>(thread->GlobalScope());
    ASSERT_TRUE(global_scope);
    ASSERT_TRUE(global_scope->IsAnimationWorkletGlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    ASSERT_TRUE(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();
    ASSERT_TRUE(isolate);

    ScriptState::Scope scope(script_state);
    global_scope->ScriptController()->Evaluate(ScriptSourceCode(
                                                   R"JS(
            registerAnimator('test', class {
              animate (currentTime, effect) {
                effect.localTime = 123;
              }
            });
          )JS"),
                                               kSharableCrossOrigin);

    // Passing a new input state with a new animation id should cause the
    // worklet to create and animate an animator.
    cc::WorkletAnimationId animation_id = {1, 1};
    AnimationWorkletInput state;
    state.added_and_updated_animations.emplace_back(animation_id, "test", 5000,
                                                    nullptr, 1);

    std::unique_ptr<AnimationWorkletOutput> output =
        global_scope->Mutate(state);
    EXPECT_TRUE(output);

    EXPECT_EQ(output->animations.size(), 1ul);
    EXPECT_EQ(output->animations[0].local_times[0],
              WTF::TimeDelta::FromMillisecondsD(123));

    waitable_event->Signal();
  }

  // This test verifies that an animator instance is not created if
  // MutatorInputState does not have an animation in
  // added_and_updated_animations.
  void RunAnimatorInstanceCreationTestOnWorklet(WorkerThread* thread,
                                                WaitableEvent* waitable_event) {
    AnimationWorkletGlobalScope* global_scope =
        static_cast<AnimationWorkletGlobalScope*>(thread->GlobalScope());
    ASSERT_TRUE(global_scope);
    ASSERT_TRUE(global_scope->IsAnimationWorkletGlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    ASSERT_TRUE(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();
    ASSERT_TRUE(isolate);

    EXPECT_EQ(global_scope->GetAnimatorsSizeForTest(), 0u);

    ScriptState::Scope scope(script_state);
    global_scope->ScriptController()->Evaluate(ScriptSourceCode(
                                                   R"JS(
            registerAnimator('test', class {
              animate (currentTime, effect) {
                effect.localTime = 123;
              }
            });
          )JS"),
                                               kSharableCrossOrigin);

    cc::WorkletAnimationId animation_id = {1, 1};
    AnimationWorkletInput state;
    state.updated_animations.push_back({animation_id, 5000});
    EXPECT_EQ(state.added_and_updated_animations.size(), 0u);
    EXPECT_EQ(state.updated_animations.size(), 1u);
    global_scope->Mutate(state);
    EXPECT_EQ(global_scope->GetAnimatorsSizeForTest(), 0u);

    state.removed_animations.push_back(animation_id);
    EXPECT_EQ(state.added_and_updated_animations.size(), 0u);
    EXPECT_EQ(state.removed_animations.size(), 1u);
    global_scope->Mutate(state);
    EXPECT_EQ(global_scope->GetAnimatorsSizeForTest(), 0u);

    state.added_and_updated_animations.push_back(
        {animation_id, "test", 5000, nullptr, 1});
    EXPECT_EQ(state.added_and_updated_animations.size(), 1u);
    global_scope->Mutate(state);
    EXPECT_EQ(global_scope->GetAnimatorsSizeForTest(), 1u);
    waitable_event->Signal();
  }

  // This test verifies that an animator instance is created and removed
  // properly.
  void RunAnimatorInstanceUpdateTestOnWorklet(WorkerThread* thread,
                                              WaitableEvent* waitable_event) {
    AnimationWorkletGlobalScope* global_scope =
        static_cast<AnimationWorkletGlobalScope*>(thread->GlobalScope());
    ASSERT_TRUE(global_scope);
    ASSERT_TRUE(global_scope->IsAnimationWorkletGlobalScope());
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    ASSERT_TRUE(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();
    ASSERT_TRUE(isolate);

    EXPECT_EQ(global_scope->GetAnimatorsSizeForTest(), 0u);

    ScriptState::Scope scope(script_state);
    global_scope->ScriptController()->Evaluate(ScriptSourceCode(
                                                   R"JS(
            registerAnimator('test', class {
              animate (currentTime, effect) {
                effect.localTime = 123;
              }
            });
          )JS"),
                                               kSharableCrossOrigin);

    cc::WorkletAnimationId animation_id = {1, 1};
    AnimationWorkletInput state;
    state.added_and_updated_animations.push_back(
        {animation_id, "test", 5000, nullptr, 1});
    EXPECT_EQ(state.added_and_updated_animations.size(), 1u);
    global_scope->Mutate(state);
    EXPECT_EQ(global_scope->GetAnimatorsSizeForTest(), 1u);

    state.added_and_updated_animations.clear();
    state.updated_animations.push_back({animation_id, 6000});
    EXPECT_EQ(state.added_and_updated_animations.size(), 0u);
    EXPECT_EQ(state.updated_animations.size(), 1u);
    global_scope->Mutate(state);
    EXPECT_EQ(global_scope->GetAnimatorsSizeForTest(), 1u);

    state.updated_animations.clear();
    state.removed_animations.push_back(animation_id);
    EXPECT_EQ(state.updated_animations.size(), 0u);
    EXPECT_EQ(state.removed_animations.size(), 1u);
    global_scope->Mutate(state);
    EXPECT_EQ(global_scope->GetAnimatorsSizeForTest(), 0u);

    waitable_event->Signal();
  }

 private:
  // Returns false when a script evaluation error happens.
  bool EvaluateScriptModule(AnimationWorkletGlobalScope* global_scope,
                            const String& source_code) {
    ScriptState* script_state =
        global_scope->ScriptController()->GetScriptState();
    EXPECT_TRUE(script_state);
    const KURL js_url("https://example.com/worklet.js");
    ScriptModule module = ScriptModule::Compile(
        script_state->GetIsolate(), source_code, js_url, js_url,
        ScriptFetchOptions(), kSharableCrossOrigin,
        TextPosition::MinimumPosition(), ASSERT_NO_EXCEPTION);
    EXPECT_FALSE(module.IsNull());
    ScriptValue exception = module.Instantiate(script_state);
    EXPECT_TRUE(exception.IsEmpty());
    ScriptValue value = module.Evaluate(script_state);
    return value.IsEmpty();
  }

  std::unique_ptr<WorkerReportingProxy> reporting_proxy_;
};

TEST_F(AnimationWorkletGlobalScopeTest, BasicParsing) {
  RunTestOnWorkletThread(
      &AnimationWorkletGlobalScopeTest::RunBasicParsingTestOnWorklet);
}

TEST_F(AnimationWorkletGlobalScopeTest, ConstructAndAnimate) {
  RunTestOnWorkletThread(
      &AnimationWorkletGlobalScopeTest::RunConstructAndAnimateTestOnWorklet);
}

TEST_F(AnimationWorkletGlobalScopeTest, AnimationOutput) {
  RunTestOnWorkletThread(
      &AnimationWorkletGlobalScopeTest::RunAnimateOutputTestOnWorklet);
}

TEST_F(AnimationWorkletGlobalScopeTest, AnimatorInstanceCreation) {
  RunTestOnWorkletThread(&AnimationWorkletGlobalScopeTest::
                             RunAnimatorInstanceCreationTestOnWorklet);
}

TEST_F(AnimationWorkletGlobalScopeTest, AnimatorInstanceUpdate) {
  RunTestOnWorkletThread(
      &AnimationWorkletGlobalScopeTest::RunAnimatorInstanceUpdateTestOnWorklet);
}

TEST_F(AnimationWorkletGlobalScopeTest,
       ShouldRegisterItselfAfterFirstAnimatorRegistration) {
  MockAnimationWorkletProxyClient* proxy_client =
      new MockAnimationWorkletProxyClient();
  std::unique_ptr<WorkerThread> worklet =
      CreateAnimationAndPaintWorkletThread(proxy_client);
  // Animation worklet global scope (AWGS) should not register itself upon
  // creation.
  EXPECT_FALSE(proxy_client->did_set_global_scope());

  WaitableEvent waitable_event;
  String source_code =
      R"JS(
        registerAnimator('test', class {
          constructor () {}
          animate () {}
        });
      )JS";
  PostCrossThreadTask(
      *worklet->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBind(&AnimationWorkletGlobalScopeTest::RunScriptOnWorklet,
                      CrossThreadUnretained(this),
                      Passed(std::move(source_code)),
                      CrossThreadUnretained(worklet.get()),
                      CrossThreadUnretained(&waitable_event)));
  waitable_event.Wait();

  // AWGS should register itself first time an animator is registered with it.
  EXPECT_TRUE(proxy_client->did_set_global_scope());

  worklet->Terminate();
  worklet->WaitForShutdownForTesting();
}

}  // namespace blink
