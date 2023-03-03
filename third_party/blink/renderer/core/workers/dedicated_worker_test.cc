// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/dedicated_worker_test.h"

#include <bitset>
#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_worker_options.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/thread_debugger_common_impl.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/wait_for_event.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_messaging_proxy.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_object_proxy.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_thread.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/parent_execution_context_task_runners.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/core/workers/worker_thread_test_helper.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class DedicatedWorkerThreadForTest final : public DedicatedWorkerThread {
 public:
  DedicatedWorkerThreadForTest(ExecutionContext* parent_execution_context,
                               DedicatedWorkerObjectProxy& worker_object_proxy)
      : DedicatedWorkerThread(
            parent_execution_context,
            worker_object_proxy,
            mojo::PendingRemote<mojom::blink::DedicatedWorkerHost>(),
            mojo::PendingRemote<
                mojom::blink::BackForwardCacheControllerHost>()) {
    worker_backing_thread_ = std::make_unique<WorkerBackingThread>(
        ThreadCreationParams(ThreadType::kTestThread));
  }

  WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams> creation_params) override {
    // Needed to avoid calling into an uninitialized broker.
    if (!creation_params->browser_interface_broker) {
      (void)creation_params->browser_interface_broker
          .InitWithNewPipeAndPassReceiver();
    }
    auto* global_scope = DedicatedWorkerGlobalScope::Create(
        std::move(creation_params), this, time_origin_,
        mojo::PendingRemote<mojom::blink::DedicatedWorkerHost>(),
        mojo::PendingRemote<mojom::blink::BackForwardCacheControllerHost>());
    // Initializing a global scope with a dummy creation params may emit warning
    // messages (e.g., invalid CSP directives).
    return global_scope;
  }

  // Emulates API use on DedicatedWorkerGlobalScope.
  void CountFeature(WebFeature feature) {
    EXPECT_TRUE(IsCurrentThread());
    GlobalScope()->CountUse(feature);
    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(&test::ExitRunLoop));
  }

  // Emulates deprecated API use on DedicatedWorkerGlobalScope.
  void CountDeprecation(WebFeature feature) {
    EXPECT_TRUE(IsCurrentThread());
    Deprecation::CountDeprecation(GlobalScope(), feature);
    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(&test::ExitRunLoop));
  }

  void TestTaskRunner() {
    EXPECT_TRUE(IsCurrentThread());
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        GlobalScope()->GetTaskRunner(TaskType::kInternalTest);
    EXPECT_TRUE(task_runner->RunsTasksInCurrentSequence());
    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(&test::ExitRunLoop));
  }

  void InitializeGlobalScope(KURL script_url) {
    EXPECT_TRUE(IsCurrentThread());
    To<DedicatedWorkerGlobalScope>(GlobalScope())
        ->Initialize(script_url, network::mojom::ReferrerPolicy::kDefault,
                     Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
                     nullptr /* response_origin_trial_tokens */);
  }
};

class DedicatedWorkerObjectProxyForTest final
    : public DedicatedWorkerObjectProxy {
 public:
  DedicatedWorkerObjectProxyForTest(
      DedicatedWorkerMessagingProxy* messaging_proxy,
      ParentExecutionContextTaskRunners* parent_execution_context_task_runners)
      : DedicatedWorkerObjectProxy(messaging_proxy,
                                   parent_execution_context_task_runners,
                                   DedicatedWorkerToken()) {}

  void CountFeature(WebFeature feature) override {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_[static_cast<size_t>(feature)]);
    reported_features_.set(static_cast<size_t>(feature));
    DedicatedWorkerObjectProxy::CountFeature(feature);
  }

 private:
  std::bitset<static_cast<size_t>(WebFeature::kNumberOfFeatures)>
      reported_features_;
};

class DedicatedWorkerMessagingProxyForTest
    : public DedicatedWorkerMessagingProxy {
 public:
  DedicatedWorkerMessagingProxyForTest(ExecutionContext* execution_context,
                                       DedicatedWorker* worker_object)
      : DedicatedWorkerMessagingProxy(
            execution_context,
            worker_object,
            [](DedicatedWorkerMessagingProxy* messaging_proxy,
               DedicatedWorker*,
               ParentExecutionContextTaskRunners* runners) {
              return std::make_unique<DedicatedWorkerObjectProxyForTest>(
                  messaging_proxy, runners);
            }) {
    script_url_ = KURL("http://fake.url/");
  }

  ~DedicatedWorkerMessagingProxyForTest() override = default;

  void StartWorker() {
    scoped_refptr<const SecurityOrigin> security_origin =
        SecurityOrigin::Create(script_url_);
    auto worker_settings = std::make_unique<WorkerSettings>(
        To<LocalDOMWindow>(GetExecutionContext())->GetFrame()->GetSettings());
    auto params = std::make_unique<GlobalScopeCreationParams>(
        script_url_, mojom::blink::ScriptType::kClassic,
        "fake global scope name", "fake user agent", UserAgentMetadata(),
        nullptr /* web_worker_fetch_context */,
        Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
        Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
        network::mojom::ReferrerPolicy::kDefault, security_origin.get(),
        false /* starter_secure_context */,
        CalculateHttpsState(security_origin.get()),
        nullptr /* worker_clients */, nullptr /* content_settings_client */,
        nullptr /* inherited_trial_features */,
        base::UnguessableToken::Create(), std::move(worker_settings),
        mojom::blink::V8CacheOptions::kDefault,
        nullptr /* worklet_module_responses_map */);
    params->parent_context_token =
        GetExecutionContext()->GetExecutionContextToken();
    InitializeWorkerThread(
        std::move(params),
        WorkerBackingThreadStartupData(
            WorkerBackingThreadStartupData::HeapLimitMode::kDefault,
            WorkerBackingThreadStartupData::AtomicsWaitMode::kAllow),
        WorkerObjectProxy().token());

    if (base::FeatureList::IsEnabled(features::kPlzDedicatedWorker)) {
      PostCrossThreadTask(
          *GetDedicatedWorkerThread()->GetTaskRunner(TaskType::kInternalTest),
          FROM_HERE,
          CrossThreadBindOnce(
              &DedicatedWorkerThreadForTest::InitializeGlobalScope,
              CrossThreadUnretained(GetDedicatedWorkerThread()), script_url_));
    }
  }

  void EvaluateClassicScript(const String& source) {
    GetWorkerThread()->EvaluateClassicScript(script_url_, source,
                                             nullptr /* cached_meta_data */,
                                             v8_inspector::V8StackTraceId());
  }

  DedicatedWorkerThreadForTest* GetDedicatedWorkerThread() {
    return static_cast<DedicatedWorkerThreadForTest*>(GetWorkerThread());
  }

  void Trace(Visitor* visitor) const override {
    DedicatedWorkerMessagingProxy::Trace(visitor);
  }

 private:
  std::unique_ptr<WorkerThread> CreateWorkerThread() override {
    return std::make_unique<DedicatedWorkerThreadForTest>(GetExecutionContext(),
                                                          WorkerObjectProxy());
  }

  KURL script_url_;
};

void DedicatedWorkerTest::SetUp() {
  PageTestBase::SetUp(gfx::Size());
  LocalDOMWindow* window = GetFrame().DomWindow();

  worker_object_ = MakeGarbageCollected<DedicatedWorker>(
      window, KURL("http://fake.url/"), WorkerOptions::Create(),
      [&](DedicatedWorker* worker) {
        auto* proxy =
            MakeGarbageCollected<DedicatedWorkerMessagingProxyForTest>(window,
                                                                       worker);
        worker_messaging_proxy_ = proxy;
        return proxy;
      });
  worker_object_->UpdateStateIfNeeded();
}

void DedicatedWorkerTest::TearDown() {
  GetWorkerThread()->TerminateForTesting();
  GetWorkerThread()->WaitForShutdownForTesting();
}

DedicatedWorkerMessagingProxyForTest*
DedicatedWorkerTest::WorkerMessagingProxy() {
  return worker_messaging_proxy_.Get();
}

DedicatedWorkerThreadForTest* DedicatedWorkerTest::GetWorkerThread() {
  return worker_messaging_proxy_->GetDedicatedWorkerThread();
}

void DedicatedWorkerTest::StartWorker() {
  WorkerMessagingProxy()->StartWorker();
}

void DedicatedWorkerTest::EvaluateClassicScript(const String& source_code) {
  WorkerMessagingProxy()->EvaluateClassicScript(source_code);
}

namespace {

void PostExitRunLoopTaskOnParent(WorkerThread* worker_thread) {
  PostCrossThreadTask(*worker_thread->GetParentTaskRunnerForTesting(),
                      FROM_HERE, CrossThreadBindOnce(&test::ExitRunLoop));
}

}  // anonymous namespace

void DedicatedWorkerTest::WaitUntilWorkerIsRunning() {
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&PostExitRunLoopTaskOnParent,
                          CrossThreadUnretained(GetWorkerThread())));

  test::EnterRunLoop();
}

TEST_F(DedicatedWorkerTest, PendingActivity_NoActivityAfterContextDestroyed) {
  StartWorker();

  EXPECT_TRUE(WorkerMessagingProxy()->HasPendingActivity());

  // Destroying the context should result in no pending activities.
  WorkerMessagingProxy()->TerminateGlobalScope();
  EXPECT_FALSE(WorkerMessagingProxy()->HasPendingActivity());
}

TEST_F(DedicatedWorkerTest, UseCounter) {
  Page::InsertOrdinaryPageForTesting(&GetPage());
  const String source_code = "// Do nothing";
  StartWorker();
  EvaluateClassicScript(source_code);

  // This feature is randomly selected.
  const WebFeature kFeature1 = WebFeature::kRequestFileSystem;

  // API use on the DedicatedWorkerGlobalScope should be recorded in UseCounter
  // on the Document.
  EXPECT_FALSE(GetDocument().IsUseCounted(kFeature1));
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&DedicatedWorkerThreadForTest::CountFeature,
                          CrossThreadUnretained(GetWorkerThread()), kFeature1));
  test::EnterRunLoop();
  EXPECT_TRUE(GetDocument().IsUseCounted(kFeature1));

  // API use should be reported to the Document only one time. See comments in
  // DedicatedWorkerObjectProxyForTest::CountFeature.
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&DedicatedWorkerThreadForTest::CountFeature,
                          CrossThreadUnretained(GetWorkerThread()), kFeature1));
  test::EnterRunLoop();

  // This feature is randomly selected from Deprecation::deprecationMessage().
  const WebFeature kFeature2 = WebFeature::kPaymentInstruments;

  // Deprecated API use on the DedicatedWorkerGlobalScope should be recorded in
  // UseCounter on the Document.
  EXPECT_FALSE(GetDocument().IsUseCounted(kFeature2));
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&DedicatedWorkerThreadForTest::CountDeprecation,
                          CrossThreadUnretained(GetWorkerThread()), kFeature2));
  test::EnterRunLoop();
  EXPECT_TRUE(GetDocument().IsUseCounted(kFeature2));

  // API use should be reported to the Document only one time. See comments in
  // DedicatedWorkerObjectProxyForTest::CountDeprecation.
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&DedicatedWorkerThreadForTest::CountDeprecation,
                          CrossThreadUnretained(GetWorkerThread()), kFeature2));
  test::EnterRunLoop();
}

TEST_F(DedicatedWorkerTest, TaskRunner) {
  StartWorker();

  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&DedicatedWorkerThreadForTest::TestTaskRunner,
                          CrossThreadUnretained(GetWorkerThread())));
  test::EnterRunLoop();
}

namespace {

BlinkTransferableMessage MakeTransferableMessage(
    base::UnguessableToken agent_cluster_id) {
  BlinkTransferableMessage message;
  message.message = SerializedScriptValue::NullValue();
  message.sender_agent_cluster_id = agent_cluster_id;
  return message;
}

}  // namespace

TEST_F(DedicatedWorkerTest, DispatchMessageEventOnWorkerObject) {
  StartWorker();

  base::RunLoop run_loop;
  auto* wait = MakeGarbageCollected<WaitForEvent>();
  wait->AddEventListener(WorkerObject(), event_type_names::kMessage);
  wait->AddEventListener(WorkerObject(), event_type_names::kMessageerror);
  wait->AddCompletionClosure(run_loop.QuitClosure());

  auto message = MakeTransferableMessage(
      GetDocument().GetExecutionContext()->GetAgentClusterID());
  WorkerMessagingProxy()->PostMessageToWorkerObject(std::move(message));
  run_loop.Run();

  EXPECT_EQ(wait->GetLastEvent()->type(), event_type_names::kMessage);
}

TEST_F(DedicatedWorkerTest,
       DispatchMessageEventOnWorkerObject_CannotDeserialize) {
  StartWorker();

  base::RunLoop run_loop;
  auto* wait = MakeGarbageCollected<WaitForEvent>();
  wait->AddEventListener(WorkerObject(), event_type_names::kMessage);
  wait->AddEventListener(WorkerObject(), event_type_names::kMessageerror);
  wait->AddCompletionClosure(run_loop.QuitClosure());

  SerializedScriptValue::ScopedOverrideCanDeserializeInForTesting
      override_can_deserialize_in(base::BindLambdaForTesting(
          [&](const SerializedScriptValue&, ExecutionContext* execution_context,
              bool can_deserialize) {
            EXPECT_EQ(execution_context, GetFrame().DomWindow());
            EXPECT_TRUE(can_deserialize);
            return false;
          }));
  auto message = MakeTransferableMessage(
      GetDocument().GetExecutionContext()->GetAgentClusterID());
  WorkerMessagingProxy()->PostMessageToWorkerObject(std::move(message));
  run_loop.Run();

  EXPECT_EQ(wait->GetLastEvent()->type(), event_type_names::kMessageerror);
}

TEST_F(DedicatedWorkerTest, DispatchMessageEventOnWorkerGlobalScope) {
  // Script must run for the worker global scope to dispatch messages.
  const String source_code = "// Do nothing";
  StartWorker();
  EvaluateClassicScript(source_code);

  AtomicString event_type;
  base::RunLoop run_loop_1;
  base::RunLoop run_loop_2;

  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(
          [](DedicatedWorkerThreadForTest* worker_thread,
             AtomicString* event_type, WTF::CrossThreadOnceClosure quit_1,
             WTF::CrossThreadOnceClosure quit_2) {
            auto* global_scope = worker_thread->GlobalScope();
            auto* wait = MakeGarbageCollected<WaitForEvent>();
            wait->AddEventListener(global_scope, event_type_names::kMessage);
            wait->AddEventListener(global_scope,
                                   event_type_names::kMessageerror);
            wait->AddCompletionClosure(WTF::BindOnce(
                [](WaitForEvent* wait, AtomicString* event_type,
                   WTF::CrossThreadOnceClosure quit_closure) {
                  *event_type = wait->GetLastEvent()->type();
                  std::move(quit_closure).Run();
                },
                WrapPersistent(wait), WTF::Unretained(event_type),
                std::move(quit_2)));
            std::move(quit_1).Run();
          },
          CrossThreadUnretained(GetWorkerThread()),
          CrossThreadUnretained(&event_type),
          WTF::CrossThreadOnceClosure(run_loop_1.QuitClosure()),
          WTF::CrossThreadOnceClosure(run_loop_2.QuitClosure())));

  // Wait for the first run loop to quit, which signals that the event listeners
  // are registered. Then post the message and wait to be notified of the
  // result. Each run loop can only be used once.
  run_loop_1.Run();
  auto message = MakeTransferableMessage(
      GetDocument().GetExecutionContext()->GetAgentClusterID());
  WorkerMessagingProxy()->PostMessageToWorkerGlobalScope(std::move(message));
  run_loop_2.Run();

  EXPECT_EQ(event_type, event_type_names::kMessage);
}

TEST_F(DedicatedWorkerTest,
       DispatchMessageEventOnWorkerGlobalScope_CannotDeserialize) {
  // Script must run for the worker global scope to dispatch messages.
  const String source_code = "// Do nothing";
  StartWorker();
  EvaluateClassicScript(source_code);

  AtomicString event_type;
  base::RunLoop run_loop_1;
  base::RunLoop run_loop_2;

  auto* worker_thread = GetWorkerThread();
  SerializedScriptValue::ScopedOverrideCanDeserializeInForTesting
      override_can_deserialize_in(base::BindLambdaForTesting(
          [&](const SerializedScriptValue&, ExecutionContext* execution_context,
              bool can_deserialize) {
            EXPECT_EQ(execution_context, worker_thread->GlobalScope());
            EXPECT_TRUE(can_deserialize);
            return false;
          }));

  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(
          [](DedicatedWorkerThreadForTest* worker_thread,
             AtomicString* event_type, WTF::CrossThreadOnceClosure quit_1,
             WTF::CrossThreadOnceClosure quit_2) {
            auto* global_scope = worker_thread->GlobalScope();
            auto* wait = MakeGarbageCollected<WaitForEvent>();
            wait->AddEventListener(global_scope, event_type_names::kMessage);
            wait->AddEventListener(global_scope,
                                   event_type_names::kMessageerror);
            wait->AddCompletionClosure(WTF::BindOnce(
                [](WaitForEvent* wait, AtomicString* event_type,
                   WTF::CrossThreadOnceClosure quit_closure) {
                  *event_type = wait->GetLastEvent()->type();
                  std::move(quit_closure).Run();
                },
                WrapPersistent(wait), WTF::Unretained(event_type),
                std::move(quit_2)));
            std::move(quit_1).Run();
          },
          CrossThreadUnretained(worker_thread),
          CrossThreadUnretained(&event_type),
          WTF::CrossThreadOnceClosure(run_loop_1.QuitClosure()),
          WTF::CrossThreadOnceClosure(run_loop_2.QuitClosure())));

  // Wait for the first run loop to quit, which signals that the event listeners
  // are registered. Then post the message and wait to be notified of the
  // result. Each run loop can only be used once.
  run_loop_1.Run();
  auto message = MakeTransferableMessage(
      GetDocument().GetExecutionContext()->GetAgentClusterID());
  WorkerMessagingProxy()->PostMessageToWorkerGlobalScope(std::move(message));
  run_loop_2.Run();

  EXPECT_EQ(event_type, event_type_names::kMessageerror);
}

}  // namespace blink
