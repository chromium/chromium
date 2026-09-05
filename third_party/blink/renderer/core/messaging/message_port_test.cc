// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/messaging/message_port.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_transfer_token.mojom-blink.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/wait_for_event.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/main_thread_worklet_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {
namespace {

BlinkTransferableMessage MakeNullMessage() {
  BlinkTransferableMessage message;
  message.message = SerializedScriptValue::NullValue();
  message.sender_agent_cluster_id = base::UnguessableToken::Create();
  return message;
}

BlinkTransferableMessage MakeWasmModuleMessage(
    ScriptState* script_state,
    const SecurityOrigin* sender_origin,
    const base::UnguessableToken& sender_agent_cluster_id) {
  BlinkTransferableMessage message = MakeNullMessage();
  // This unit test exercises dispatch with a real compiled Wasm attachment.
  // The WPT coverage exercises its serialization end to end.
  static constexpr uint8_t kEmptyWasmModuleBytes[] = {0x00, 0x61, 0x73, 0x6d,
                                                      0x01, 0x00, 0x00, 0x00};
  ScriptState::Scope scope(script_state);
  v8::Local<v8::WasmModuleObject> module =
      v8::WasmModuleObject::Compile(script_state->GetIsolate(),
                                    kEmptyWasmModuleBytes)
          .ToLocalChecked();
  message.message->WasmModules().push_back(module->GetCompiledModule());
  message.sender_origin = sender_origin->IsolatedCopy();
  message.sender_agent_cluster_id = sender_agent_cluster_id;
  message.locked_to_sender_agent_cluster =
      message.message->IsLockedToAgentCluster();
  EXPECT_EQ(
      message.message->GetOriginCheckRequirement(),
      SerializedScriptValue::OriginCheckRequirement::kAllowRelatedAudioWorklet);
  EXPECT_TRUE(message.message->IsOriginCheckRequired());
  EXPECT_TRUE(message.locked_to_sender_agent_cluster);
  return message;
}

void AddFileSystemAccessToken(BlinkTransferableMessage& message) {
  mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken> token;
  token.InitWithNewPipeAndPassReceiver().reset();
  message.message->FileSystemAccessTokens().push_back(std::move(token));
}

class FakeAudioWorkletGlobalScopeForMessagePortTest final
    : public WorkletGlobalScope {
 public:
  FakeAudioWorkletGlobalScopeForMessagePortTest(
      std::unique_ptr<GlobalScopeCreationParams> creation_params,
      WorkerReportingProxy& reporting_proxy,
      LocalFrame* frame)
      : WorkletGlobalScope(std::move(creation_params), reporting_proxy, frame) {
  }

  bool IsAudioWorkletGlobalScope() const final { return true; }
  WorkletToken GetWorkletToken() const final { return token_; }
  ExecutionContextToken GetExecutionContextToken() const final {
    return token_;
  }

 private:
  network::mojom::RequestDestination GetDestination() const final {
    return network::mojom::RequestDestination::kScript;
  }

  const AudioWorkletToken token_;
};

class MessagePortAudioWorkletOriginTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    NavigateTo(KURL("https://example.test/"));
    reporting_proxy_ = std::make_unique<MainThreadWorkletReportingProxy>(
        GetFrame().DomWindow());
  }

  void TearDown() override {
    if (global_scope_) {
      global_scope_->Dispose();
      global_scope_->NotifyContextDestroyed();
    }
    reporting_proxy_.reset();
    PageTestBase::TearDown();
  }

 protected:
  FakeAudioWorkletGlobalScopeForMessagePortTest* CreateGlobalScope() {
    LocalDOMWindow* window = GetFrame().DomWindow();
    auto creation_params = std::make_unique<GlobalScopeCreationParams>(
        window->Url(), mojom::blink::ScriptType::kModule, "TestWorklet",
        window->UserAgent(), UserAgentMetadata(),
        nullptr /* web_worker_fetch_context */,
        Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
        Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
        window->GetReferrerPolicy(), DocumentPolicy::DocumentPolicyBundle{},
        window->GetSecurityOrigin(), window->IsSecureContext(),
        window->GetHttpsState(), nullptr /* worker_clients */,
        nullptr /* content_settings_client */,
        nullptr /* inherited_trial_features */,
        base::UnguessableToken::Create(), nullptr /* worker_settings */,
        mojom::blink::V8CacheOptions::kDefault,
        MakeGarbageCollected<WorkletModuleResponsesMap>(),
        mojo::NullRemote() /* browser_interface_broker */,
        mojo::NullRemote() /* code_cache_host */,
        mojo::NullRemote() /* blob_url_store */, BeginFrameProviderParams(),
        nullptr /* parent_permissions_policy */, window->GetAgentClusterID());
    global_scope_ =
        MakeGarbageCollected<FakeAudioWorkletGlobalScopeForMessagePortTest>(
            std::move(creation_params), *reporting_proxy_, &GetFrame());
    return global_scope_.Get();
  }

  AtomicString DispatchMessage(ExecutionContext& context,
                               BlinkTransferableMessage message) {
    MessagePort* port = MakeGarbageCollected<MessagePort>(context);
    base::RunLoop run_loop;
    auto* wait = MakeGarbageCollected<WaitForEvent>();
    wait->AddEventListener(port, event_type_names::kMessage);
    wait->AddEventListener(port, event_type_names::kMessageerror);
    wait->AddCompletionClosure(run_loop.QuitClosure());

    mojo::Message mojo_message =
        mojom::blink::TransferableMessage::WrapAsMessage(std::move(message));
    if (!static_cast<mojo::MessageReceiver*>(port)->Accept(&mojo_message)) {
      ADD_FAILURE() << "Failed to deserialize the transferable message";
      return AtomicString();
    }
    run_loop.Run();
    return wait->GetLastEvent()->type();
  }

 private:
  std::unique_ptr<MainThreadWorkletReportingProxy> reporting_proxy_;
  Persistent<FakeAudioWorkletGlobalScopeForMessagePortTest> global_scope_;
};

TEST(MessagePortTest, DispatchMessageEvent) {
  test::TaskEnvironment task_environment;
  DummyPageHolder holder;
  LocalDOMWindow* window = holder.GetFrame().DomWindow();

  MessagePort* port = MakeGarbageCollected<MessagePort>(*window);

  base::RunLoop run_loop;
  auto* wait = MakeGarbageCollected<WaitForEvent>();
  wait->AddEventListener(port, event_type_names::kMessage);
  wait->AddEventListener(port, event_type_names::kMessageerror);
  wait->AddCompletionClosure(run_loop.QuitClosure());

  mojo::Message mojo_message =
      mojom::blink::TransferableMessage::WrapAsMessage(MakeNullMessage());
  ASSERT_TRUE(static_cast<mojo::MessageReceiver*>(port)->Accept(&mojo_message));
  run_loop.Run();

  EXPECT_EQ(wait->GetLastEvent()->type(), event_type_names::kMessage);
}

TEST(MessagePortTest, DispatchMessageErrorEvent_LockedAgentCluster) {
  test::TaskEnvironment task_environment;
  DummyPageHolder holder;
  LocalDOMWindow* window = holder.GetFrame().DomWindow();

  MessagePort* port = MakeGarbageCollected<MessagePort>(*window);

  base::RunLoop run_loop;
  auto* wait = MakeGarbageCollected<WaitForEvent>();
  wait->AddEventListener(port, event_type_names::kMessage);
  wait->AddEventListener(port, event_type_names::kMessageerror);
  wait->AddCompletionClosure(run_loop.QuitClosure());

  auto message = MakeNullMessage();
  message.locked_to_sender_agent_cluster = true;
  mojo::Message mojo_message =
      mojom::blink::TransferableMessage::WrapAsMessage(std::move(message));
  ASSERT_TRUE(static_cast<mojo::MessageReceiver*>(port)->Accept(&mojo_message));
  run_loop.Run();

  EXPECT_EQ(wait->GetLastEvent()->type(), event_type_names::kMessageerror);
}

TEST(MessagePortTest, DispatchMessageErrorEvent_CannotDeserialize) {
  test::TaskEnvironment task_environment;
  DummyPageHolder holder;
  LocalDOMWindow* window = holder.GetFrame().DomWindow();
  MessagePort* port = MakeGarbageCollected<MessagePort>(*window);

  SerializedScriptValue::ScopedOverrideCanDeserializeInForTesting
      override_can_deserialize_in(base::BindLambdaForTesting(
          [&](const SerializedScriptValue& value,
              ExecutionContext* execution_context, bool can_deserialize) {
            EXPECT_EQ(execution_context, window);
            EXPECT_TRUE(can_deserialize);
            return false;
          }));

  base::RunLoop run_loop;
  auto* wait = MakeGarbageCollected<WaitForEvent>();
  wait->AddEventListener(port, event_type_names::kMessage);
  wait->AddEventListener(port, event_type_names::kMessageerror);
  wait->AddCompletionClosure(run_loop.QuitClosure());

  mojo::Message mojo_message =
      mojom::blink::TransferableMessage::WrapAsMessage(MakeNullMessage());
  ASSERT_TRUE(static_cast<mojo::MessageReceiver*>(port)->Accept(&mojo_message));
  run_loop.Run();

  EXPECT_EQ(wait->GetLastEvent()->type(), event_type_names::kMessageerror);
}

TEST_F(MessagePortAudioWorkletOriginTest,
       AudioWorkletUsesCreatorOriginForWasmModules) {
  auto* scope = CreateGlobalScope();
  ASSERT_TRUE(scope->GetSecurityOrigin()->IsOpaque());
  ASSERT_TRUE(scope->DocumentSecurityOrigin());
  ASSERT_FALSE(scope->DocumentSecurityOrigin()->IsOpaque());
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());

  EXPECT_EQ(
      DispatchMessage(*scope, MakeWasmModuleMessage(
                                  script_state, scope->DocumentSecurityOrigin(),
                                  scope->GetAgentClusterID())),
      event_type_names::kMessage);
}

TEST_F(MessagePortAudioWorkletOriginTest,
       AudioWorkletRejectsWasmModulesFromDifferentOrigin) {
  auto* scope = CreateGlobalScope();
  scoped_refptr<SecurityOrigin> other_origin =
      SecurityOrigin::Create(KURL("https://other.test/"));
  ASSERT_FALSE(
      scope->DocumentSecurityOrigin()->IsSameOriginWith(other_origin.get()));
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());

  EXPECT_EQ(DispatchMessage(
                *scope, MakeWasmModuleMessage(script_state, other_origin.get(),
                                              scope->GetAgentClusterID())),
            event_type_names::kMessageerror);
}

TEST_F(MessagePortAudioWorkletOriginTest,
       AudioWorkletRejectsWasmModulesFromDifferentAgentCluster) {
  auto* scope = CreateGlobalScope();
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());

  EXPECT_EQ(
      DispatchMessage(*scope, MakeWasmModuleMessage(
                                  script_state, scope->DocumentSecurityOrigin(),
                                  base::UnguessableToken::Create())),
      event_type_names::kMessageerror);
}

TEST_F(MessagePortAudioWorkletOriginTest,
       AudioWorkletRejectsFileSystemAccessTokensFromCreatorOrigin) {
  auto* scope = CreateGlobalScope();
  BlinkTransferableMessage message = MakeNullMessage();
  AddFileSystemAccessToken(message);
  message.sender_origin = scope->DocumentSecurityOrigin()->IsolatedCopy();
  ASSERT_TRUE(message.message->IsOriginCheckRequired());
  ASSERT_EQ(message.message->GetOriginCheckRequirement(),
            SerializedScriptValue::OriginCheckRequirement::kStrict);

  EXPECT_EQ(DispatchMessage(*scope, std::move(message)),
            event_type_names::kMessageerror);
}

TEST_F(MessagePortAudioWorkletOriginTest,
       AudioWorkletRejectsMixedOriginBoundDataFromCreatorOrigin) {
  auto* scope = CreateGlobalScope();
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  BlinkTransferableMessage message =
      MakeWasmModuleMessage(script_state, scope->DocumentSecurityOrigin(),
                            scope->GetAgentClusterID());
  AddFileSystemAccessToken(message);
  ASSERT_TRUE(message.message->IsOriginCheckRequired());
  ASSERT_EQ(message.message->GetOriginCheckRequirement(),
            SerializedScriptValue::OriginCheckRequirement::kStrict);

  EXPECT_EQ(DispatchMessage(*scope, std::move(message)),
            event_type_names::kMessageerror);
}

}  // namespace
}  // namespace blink
