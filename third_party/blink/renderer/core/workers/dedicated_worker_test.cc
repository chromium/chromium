// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/dedicated_worker_test.h"

#include <bitset>
#include <cstddef>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_message_port.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_post_message_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_worker_options.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/thread_debugger_common_impl.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/messaging/message_channel.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/wait_for_event.h"
#include "third_party/blink/renderer/core/workers/custom_event_message.h"
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
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "v8-value.h"

namespace blink {

namespace {

constexpr char kCustomEventName[] = "custom";
constexpr char kCustomErrorEventName[] = "customerror";

class CustomEventWithData final : public Event {
 public:
  explicit CustomEventWithData(const AtomicString& event_type)
      : Event(event_type, Bubbles::kNo, Cancelable::kNo) {}
  explicit CustomEventWithData(const AtomicString& event_type,
                               scoped_refptr<SerializedScriptValue> data)
      : CustomEventWithData(event_type, std::move(data), nullptr) {}

  explicit CustomEventWithData(const AtomicString& event_type,
                               scoped_refptr<SerializedScriptValue> data,
                               MessagePortArray* ports)
      : Event(event_type, Bubbles::kNo, Cancelable::kNo),
        data_as_serialized_script_value_(
            SerializedScriptValue::Unpack(std::move(data))),
        ports_(ports) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(data_as_serialized_script_value_);
    visitor->Trace(ports_);
    Event::Trace(visitor);
  }
  SerializedScriptValue* DataAsSerializedScriptValue() const {
    if (!data_as_serialized_script_value_) {
      return nullptr;
    }
    return data_as_serialized_script_value_->Value();
  }

  MessagePortArray* ports() { return ports_; }

 private:
  Member<UnpackedSerializedScriptValue> data_as_serialized_script_value_;
  Member<MessagePortArray> ports_;
};

ScriptValue CreateStringScriptValue(ScriptState* script_state,
                                    const String& str) {
  return ScriptValue(script_state->GetIsolate(),
                     V8String(script_state->GetIsolate(), str));
}

CrossThreadFunction<Event*(ScriptState*, CustomEventMessage)>
CustomEventFactoryCallback(base::RepeatingClosure quit_closure,
                           CustomEventWithData** out_event = nullptr) {
  return CrossThreadBindRepeating(base::BindLambdaForTesting(
      [quit_closure = std::move(quit_closure), out_event](
          ScriptState*, CustomEventMessage data) -> Event* {
        CustomEventWithData* result = MakeGarbageCollected<CustomEventWithData>(
            AtomicString::FromUTF8(kCustomEventName), std::move(data.message));
        if (out_event) {
          *out_event = result;
        }
        quit_closure.Run();
        return result;
      }));
}

CrossThreadFunction<Event*(ScriptState*)> CustomEventFactoryErrorCallback(
    base::RepeatingClosure quit_closure,
    Event** out_event = nullptr) {
  return CrossThreadBindRepeating(base::BindLambdaForTesting(
      [quit_closure = std::move(quit_closure), out_event](ScriptState*) {
        Event* result = MakeGarbageCollected<CustomEventWithData>(
            AtomicString::FromUTF8(kCustomErrorEventName));
        if (out_event) {
          *out_event = result;
        }
        quit_closure.Run();
        return result;
      }));
}

CrossThreadFunction<Event*(ScriptState*, CustomEventMessage)>
CustomEventWithPortsFactoryCallback(base::RepeatingClosure quit_closure,
                                    CustomEventWithData** out_event = nullptr) {
  return CrossThreadBindRepeating(base::BindLambdaForTesting(
      [quit_closure = std::move(quit_closure), out_event](
          ScriptState* script_state, CustomEventMessage message) -> Event* {
        MessagePortArray* ports = MessagePort::EntanglePorts(
            *ExecutionContext::From(script_state), std::move(message.ports));
        CustomEventWithData* result = MakeGarbageCollected<CustomEventWithData>(
            AtomicString::FromUTF8(kCustomEventName),
            std::move(message.message), ports);
        if (out_event) {
          *out_event = result;
        }
        quit_closure.Run();
        return result;
      }));
}

}  // namespace

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
  void CountFeature(WebFeature feature, CrossThreadOnceClosure quit_closure) {
    EXPECT_TRUE(IsCurrentThread());
    GlobalScope()->CountUse(feature);
    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(std::move(quit_closure)));
  }
  void CountWebDXFeature(WebDXFeature feature,
                         CrossThreadOnceClosure quit_closure) {
    EXPECT_TRUE(IsCurrentThread());
    GlobalScope()->CountWebDXFeature(feature);
    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(std::move(quit_closure)));
  }

  // Emulates deprecated API use on DedicatedWorkerGlobalScope.
  void CountDeprecation(WebFeature feature,
                        CrossThreadOnceClosure quit_closure) {
    EXPECT_TRUE(IsCurrentThread());
    Deprecation::CountDeprecation(GlobalScope(), feature);
    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(std::move(quit_closure)));
  }

  void TestTaskRunner(CrossThreadOnceClosure quit_closure) {
    EXPECT_TRUE(IsCurrentThread());
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        GlobalScope()->GetTaskRunner(TaskType::kInternalTest);
    EXPECT_TRUE(task_runner->RunsTasksInCurrentSequence());
    PostCrossThreadTask(*GetParentTaskRunnerForTesting(), FROM_HERE,
                        CrossThreadBindOnce(std::move(quit_closure)));
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

  void CountWebDXFeature(WebDXFeature feature) override {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_webdx_features_[static_cast<size_t>(feature)]);
    reported_webdx_features_.set(static_cast<size_t>(feature));
    DedicatedWorkerObjectProxy::CountWebDXFeature(feature);
  }

 private:
  std::bitset<static_cast<size_t>(WebFeature::kMaxValue) + 1>
      reported_features_;
  std::bitset<static_cast<size_t>(WebDXFeature::kMaxValue) + 1>
      reported_webdx_features_;
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

  void StartWorker(
      std::unique_ptr<GlobalScopeCreationParams> params = nullptr) {
    scoped_refptr<const SecurityOrigin> security_origin =
        SecurityOrigin::Create(script_url_);
    auto worker_settings = std::make_unique<WorkerSettings>(
        To<LocalDOMWindow>(GetExecutionContext())->GetFrame()->GetSettings());
    if (!params) {
      params = std::make_unique<GlobalScopeCreationParams>(
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
    }
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

  const KURL& script_url() const { return script_url_; }

 private:
  std::unique_ptr<WorkerThread> CreateWorkerThread() override {
    return std::make_unique<DedicatedWorkerThreadForTest>(GetExecutionContext(),
                                                          WorkerObjectProxy());
  }

  KURL script_url_;
};

class FakeWebDedicatedWorkerHostFactoryClient
    : public WebDedicatedWorkerHostFactoryClient {
 public:
  // Implements WebDedicatedWorkerHostFactoryClient.
  void CreateWorkerHostDeprecated(
      const DedicatedWorkerToken& dedicated_worker_token,
      const WebURL& script_url,
      const WebSecurityOrigin& origin,
      CreateWorkerHostCallback callback) override {}
  void CreateWorkerHost(
      const DedicatedWorkerToken& dedicated_worker_token,
      const WebURL& script_url,
      network::mojom::CredentialsMode credentials_mode,
      const WebFetchClientSettingsObject& fetch_client_settings_object,
      CrossVariantMojoRemote<blink::mojom::BlobURLTokenInterfaceBase>
          blob_url_token,
      net::StorageAccessApiStatus storage_access_api_status) override {}
  scoped_refptr<blink::WebWorkerFetchContext> CloneWorkerFetchContext(
      WebWorkerFetchContext* web_worker_fetch_context,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    return nullptr;
  }
};

class FakeWebDedicatedWorkerHostFactoryClientPlatformSupport
    : public TestingPlatformSupport {
 public:
  std::unique_ptr<blink::WebDedicatedWorkerHostFactoryClient>
  CreateDedicatedWorkerHostFactoryClient(
      WebDedicatedWorker* worker,
      const BrowserInterfaceBrokerProxy& interface_broker) override {
    return std::make_unique<FakeWebDedicatedWorkerHostFactoryClient>();
  }
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

void DedicatedWorkerTest::StartWorker(
    std::unique_ptr<GlobalScopeCreationParams> params) {
  WorkerMessagingProxy()->StartWorker(std::move(params));
}

void DedicatedWorkerTest::EvaluateClassicScript(const String& source_code) {
  WorkerMessagingProxy()->EvaluateClassicScript(source_code);
}

namespace {

void PostExitRunLoopTaskOnParent(WorkerThread* worker_thread,
                                 CrossThreadOnceClosure quit_closure) {
  PostCrossThreadTask(*worker_thread->GetParentTaskRunnerForTesting(),
                      FROM_HERE, CrossThreadBindOnce(std::move(quit_closure)));
}

}  // anonymous namespace

void DedicatedWorkerTest::WaitUntilWorkerIsRunning() {
  base::RunLoop loop;
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&PostExitRunLoopTaskOnParent,
                          CrossThreadUnretained(GetWorkerThread()),
                          CrossThreadBindOnce(loop.QuitClosure())));

  loop.Run();
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
  const WebDXFeature kWebDXFeature1 = WebDXFeature::kCompressionStreams;

  // API use on the DedicatedWorkerGlobalScope should be recorded in UseCounter
  // on the Document.
  EXPECT_FALSE(GetDocument().IsUseCounted(kFeature1));
  {
    base::RunLoop loop;
    PostCrossThreadTask(
        *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(&DedicatedWorkerThreadForTest::CountFeature,
                            CrossThreadUnretained(GetWorkerThread()), kFeature1,
                            CrossThreadBindOnce(loop.QuitClosure())));
    loop.Run();
  }
  EXPECT_TRUE(GetDocument().IsUseCounted(kFeature1));

  EXPECT_FALSE(GetDocument().IsWebDXFeatureCounted(kWebDXFeature1));
  {
    base::RunLoop loop;
    PostCrossThreadTask(
        *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(&DedicatedWorkerThreadForTest::CountWebDXFeature,
                            CrossThreadUnretained(GetWorkerThread()),
                            kWebDXFeature1,
                            CrossThreadBindOnce(loop.QuitClosure())));
    loop.Run();
  }
  EXPECT_TRUE(GetDocument().IsWebDXFeatureCounted(kWebDXFeature1));

  // API use should be reported to the Document only one time. See comments in
  // DedicatedWorkerObjectProxyForTest::CountFeature.
  {
    base::RunLoop loop;
    PostCrossThreadTask(
        *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(&DedicatedWorkerThreadForTest::CountFeature,
                            CrossThreadUnretained(GetWorkerThread()), kFeature1,
                            CrossThreadBindOnce(loop.QuitClosure())));
    loop.Run();
  }

  // This feature is randomly selected from Deprecation::deprecationMessage().
  const WebFeature kFeature2 = WebFeature::kPaymentInstruments;

  // Deprecated API use on the DedicatedWorkerGlobalScope should be recorded in
  // UseCounter on the Document.
  EXPECT_FALSE(GetDocument().IsUseCounted(kFeature2));
  {
    base::RunLoop loop;
    PostCrossThreadTask(
        *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(&DedicatedWorkerThreadForTest::CountDeprecation,
                            CrossThreadUnretained(GetWorkerThread()), kFeature2,
                            CrossThreadBindOnce(loop.QuitClosure())));
    loop.Run();
  }
  EXPECT_TRUE(GetDocument().IsUseCounted(kFeature2));

  // API use should be reported to the Document only one time. See comments in
  // DedicatedWorkerObjectProxyForTest::CountDeprecation.
  {
    base::RunLoop loop;
    PostCrossThreadTask(
        *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
        CrossThreadBindOnce(&DedicatedWorkerThreadForTest::CountDeprecation,
                            CrossThreadUnretained(GetWorkerThread()), kFeature2,
                            CrossThreadBindOnce(loop.QuitClosure())));
    loop.Run();
  }
}

TEST_F(DedicatedWorkerTest, TaskRunner) {
  base::RunLoop loop;
  StartWorker();

  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(&DedicatedWorkerThreadForTest::TestTaskRunner,
                          CrossThreadUnretained(GetWorkerThread()),
                          CrossThreadBindOnce(loop.QuitClosure())));
  loop.Run();
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

TEST_F(DedicatedWorkerTest, TopLevelFrameSecurityOrigin) {
  ScopedTestingPlatformSupport<
      FakeWebDedicatedWorkerHostFactoryClientPlatformSupport>
      platform;
  const auto& script_url = WorkerMessagingProxy()->script_url();
  scoped_refptr<SecurityOrigin> security_origin =
      SecurityOrigin::Create(script_url);
  WorkerObject()
      ->GetExecutionContext()
      ->GetSecurityContext()
      .SetSecurityOriginForTesting(security_origin);
  StartWorker(WorkerObject()->CreateGlobalScopeCreationParams(
      script_url, network::mojom::ReferrerPolicy::kDefault,
      Vector<network::mojom::blink::ContentSecurityPolicyPtr>()));
  base::RunLoop run_loop;

  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kInternalTest), FROM_HERE,
      CrossThreadBindOnce(
          [](DedicatedWorkerThreadForTest* worker_thread,
             WTF::CrossThreadOnceClosure quit,
             const SecurityOrigin* security_origin, const KURL& script_url) {
            // Check the worker's top level frame security origin.
            auto* worker_global_scope =
                static_cast<WorkerGlobalScope*>(worker_thread->GlobalScope());
            ASSERT_TRUE(worker_global_scope->top_level_frame_security_origin());
            EXPECT_TRUE(worker_global_scope->top_level_frame_security_origin()
                            ->IsSameOriginDomainWith(security_origin));

            // Create a nested worker and check the top level frame security
            // origin of the GlobalScopeCreationParams.
            {
              auto* nested_worker_object =
                  MakeGarbageCollected<DedicatedWorker>(
                      worker_global_scope, script_url, WorkerOptions::Create());
              nested_worker_object->UpdateStateIfNeeded();

              auto nested_worker_params =
                  nested_worker_object->CreateGlobalScopeCreationParams(
                      script_url, network::mojom::ReferrerPolicy::kDefault,
                      Vector<
                          network::mojom::blink::ContentSecurityPolicyPtr>());
              ASSERT_TRUE(
                  nested_worker_params->top_level_frame_security_origin);
              EXPECT_TRUE(nested_worker_params->top_level_frame_security_origin
                              ->IsSameOriginDomainWith(security_origin));
            }
            std::move(quit).Run();
          },
          CrossThreadUnretained(GetWorkerThread()),
          WTF::CrossThreadOnceClosure(run_loop.QuitClosure()),
          CrossThreadUnretained(WorkerObject()
                                    ->GetExecutionContext()
                                    ->GetSecurityContext()
                                    .GetSecurityOrigin()),
          script_url));
  run_loop.Run();
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

TEST_F(DedicatedWorkerTest, PostCustomEventWithString) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();

  StartWorker();
  EvaluateClassicScript("");
  WaitUntilWorkerIsRunning();

  base::RunLoop run_loop;
  HeapVector<ScriptValue> transfer;
  CustomEventWithData* event = nullptr;
  String data = "postEventWithDataTesting";
  WorkerObject()->PostCustomEvent(
      TaskType::kPostedMessage, script_state,
      CustomEventFactoryCallback(run_loop.QuitClosure(), &event),
      CustomEventFactoryErrorCallback(run_loop.QuitClosure()),
      CreateStringScriptValue(script_state, data), transfer,
      v8_scope.GetExceptionState());
  run_loop.Run();

  ASSERT_NE(event, nullptr);
  EXPECT_EQ(event->type(), kCustomEventName);
  v8::Local<v8::Value> value =
      event->DataAsSerializedScriptValue()->Deserialize(
          v8_scope.GetIsolate(), SerializedScriptValue::DeserializeOptions());
  String result;
  ScriptValue(v8_scope.GetIsolate(), value).ToString(result);
  EXPECT_EQ(result, data);
}

TEST_F(DedicatedWorkerTest, PostCustomEventWithNumber) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();

  StartWorker();
  EvaluateClassicScript("");
  WaitUntilWorkerIsRunning();

  base::RunLoop run_loop;
  HeapVector<ScriptValue> transfer;
  CustomEventWithData* event = nullptr;
  const double kNumber = 2.34;
  v8::Local<v8::Value> v8_number =
      v8::Number::New(v8_scope.GetIsolate(), kNumber);

  WorkerObject()->PostCustomEvent(
      TaskType::kPostedMessage, script_state,
      CustomEventFactoryCallback(run_loop.QuitClosure(), &event),
      CustomEventFactoryErrorCallback(run_loop.QuitClosure()),
      ScriptValue(script_state->GetIsolate(), v8_number), transfer,
      v8_scope.GetExceptionState());
  run_loop.Run();

  ASSERT_NE(event, nullptr);
  EXPECT_EQ(event->type(), kCustomEventName);
  v8::Local<v8::Value> value =
      static_cast<CustomEventWithData*>(event)
          ->DataAsSerializedScriptValue()
          ->Deserialize(v8_scope.GetIsolate(),
                        SerializedScriptValue::DeserializeOptions());
  EXPECT_EQ(ScriptValue(v8_scope.GetIsolate(), value)
                .V8Value()
                .As<v8::Number>()
                ->Value(),
            kNumber);
}

TEST_F(DedicatedWorkerTest, PostCustomEventBeforeWorkerStarts) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();

  base::RunLoop run_loop;
  HeapVector<ScriptValue> transfer;
  CustomEventWithData* event = nullptr;
  String data = "postEventWithDataTesting";
  WorkerObject()->PostCustomEvent(
      TaskType::kPostedMessage, script_state,
      CustomEventFactoryCallback(run_loop.QuitClosure(), &event),
      CustomEventFactoryErrorCallback(run_loop.QuitClosure()),
      CreateStringScriptValue(script_state, data), transfer,
      v8_scope.GetExceptionState());

  StartWorker();
  EvaluateClassicScript("");
  WaitUntilWorkerIsRunning();
  run_loop.Run();
  ASSERT_NE(event, nullptr);

  EXPECT_EQ(event->type(), kCustomEventName);
  v8::Local<v8::Value> value =
      event->DataAsSerializedScriptValue()->Deserialize(
          v8_scope.GetIsolate(), SerializedScriptValue::DeserializeOptions());
  String result;
  EXPECT_TRUE(ScriptValue(v8_scope.GetIsolate(), value).ToString(result));
  EXPECT_EQ(result, data);
}

TEST_F(DedicatedWorkerTest, PostCustomEventWithPort) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();

  StartWorker();
  EvaluateClassicScript("");
  WaitUntilWorkerIsRunning();

  MessageChannel* channel =
      MakeGarbageCollected<MessageChannel>(v8_scope.GetExecutionContext());
  ScriptValue script_value =
      ScriptValue::From(v8_scope.GetScriptState(), channel->port1());
  HeapVector<ScriptValue> transfer = {script_value};
  CustomEventWithData* event = nullptr;
  base::RunLoop run_loop;

  WorkerObject()->PostCustomEvent(
      TaskType::kPostedMessage, script_state,
      CustomEventWithPortsFactoryCallback(run_loop.QuitClosure(), &event),
      CustomEventFactoryErrorCallback(run_loop.QuitClosure()), script_value,
      transfer, v8_scope.GetExceptionState());
  run_loop.Run();

  ASSERT_NE(event, nullptr);
  EXPECT_EQ(event->type(), kCustomEventName);
  ASSERT_FALSE(event->ports()->empty());
  EXPECT_NE(event->ports()->at(0), nullptr);
}

TEST_F(DedicatedWorkerTest, PostCustomEventCannotDeserialize) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();

  StartWorker();
  EvaluateClassicScript("");
  WaitUntilWorkerIsRunning();

  auto* worker_thread = GetWorkerThread();
  SerializedScriptValue::ScopedOverrideCanDeserializeInForTesting
      override_can_deserialize_in(base::BindLambdaForTesting(
          [&](const SerializedScriptValue&, ExecutionContext* execution_context,
              bool can_deserialize) {
            EXPECT_EQ(execution_context, worker_thread->GlobalScope());
            EXPECT_TRUE(can_deserialize);
            return false;
          }));
  base::RunLoop run_loop;
  HeapVector<ScriptValue> transfer;
  String data = "postEventWithDataTesting";
  Event* event = nullptr;
  WorkerObject()->PostCustomEvent(
      TaskType::kPostedMessage, script_state,
      CustomEventFactoryCallback(run_loop.QuitClosure()),
      CustomEventFactoryErrorCallback(run_loop.QuitClosure(), &event),
      CreateStringScriptValue(script_state, data), transfer,
      v8_scope.GetExceptionState());
  run_loop.Run();
  EXPECT_EQ(event->type(), kCustomErrorEventName);
}

TEST_F(DedicatedWorkerTest, PostCustomEventNoMessage) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();

  StartWorker();
  EvaluateClassicScript("");
  WaitUntilWorkerIsRunning();

  base::RunLoop run_loop;
  HeapVector<ScriptValue> transfer;
  CustomEventWithData* event = nullptr;

  WorkerObject()->PostCustomEvent(
      TaskType::kPostedMessage, script_state,
      CustomEventFactoryCallback(run_loop.QuitClosure(), &event),
      CustomEventFactoryErrorCallback(run_loop.QuitClosure()), ScriptValue(),
      transfer, v8_scope.GetExceptionState());
  run_loop.Run();

  ASSERT_NE(event, nullptr);
  EXPECT_EQ(event->type(), kCustomEventName);
  EXPECT_EQ(event->DataAsSerializedScriptValue(), nullptr);
  EXPECT_EQ(event->ports(), nullptr);
}

}  // namespace blink
