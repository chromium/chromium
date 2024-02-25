// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/broadcastchannel/broadcast_channel.h"

#include <iterator>

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/messaging/blink_cloneable_message.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

class BroadcastChannelTester : public GarbageCollected<BroadcastChannelTester>,
                               public mojom::blink::BroadcastChannelClient {
 public:
  explicit BroadcastChannelTester(ExecutionContext* execution_context)
      : receiver_(this, execution_context), remote_(execution_context) {
    // Ideally, these would share a pipe. This is more convenient.
    mojo::PendingAssociatedReceiver<mojom::blink::BroadcastChannelClient>
        receiver0;
    mojo::PendingAssociatedRemote<mojom::blink::BroadcastChannelClient>
        remote0 = receiver0.InitWithNewEndpointAndPassRemote();
    receiver0.EnableUnassociatedUsage();
    mojo::PendingAssociatedReceiver<mojom::blink::BroadcastChannelClient>
        receiver1;
    mojo::PendingAssociatedRemote<mojom::blink::BroadcastChannelClient>
        remote1 = receiver1.InitWithNewEndpointAndPassRemote();
    receiver1.EnableUnassociatedUsage();

    scoped_refptr<base::SequencedTaskRunner> task_runner =
        execution_context->GetTaskRunner(TaskType::kInternalTest);
    receiver_.Bind(std::move(receiver0), task_runner);
    remote_.Bind(std::move(remote1), task_runner);
    channel_ = MakeGarbageCollected<BroadcastChannel>(
        base::PassKey<BroadcastChannelTester>(), execution_context,
        "BroadcastChannelTester", std::move(receiver1), std::move(remote0));

    auto* listener = MakeGarbageCollected<EventListener>(this);
    channel_->addEventListener(event_type_names::kMessage, listener);
    channel_->addEventListener(event_type_names::kMessageerror, listener);
  }

  BroadcastChannel* channel() const { return channel_.Get(); }
  const HeapVector<Member<MessageEvent>>& received_events() const {
    return received_events_;
  }
  const Vector<BlinkCloneableMessage>& sent_messages() const {
    return sent_messages_;
  }

  void AwaitNextUpdate(base::OnceClosure closure) {
    on_next_update_.push_back(std::move(closure));
  }

  void PostMessage(BlinkCloneableMessage message) {
    remote_->OnMessage(std::move(message));
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(channel_);
    visitor->Trace(received_events_);
    visitor->Trace(receiver_);
    visitor->Trace(remote_);
  }

  // BroadcastChannelClient
  void OnMessage(BlinkCloneableMessage message) override {
    sent_messages_.push_back(std::move(message));
    ReportUpdate();
  }

 private:
  class EventListener : public NativeEventListener {
   public:
    explicit EventListener(BroadcastChannelTester* tester) : tester_(tester) {}
    void Trace(Visitor* visitor) const override {
      NativeEventListener::Trace(visitor);
      visitor->Trace(tester_);
    }

    void Invoke(ExecutionContext*, Event* event) override {
      tester_->received_events_.push_back(static_cast<MessageEvent*>(event));
      tester_->ReportUpdate();
    }

   private:
    Member<BroadcastChannelTester> tester_;
  };

  void ReportUpdate() {
    Vector<base::OnceClosure> closures = std::move(on_next_update_);
    for (auto& closure : closures)
      std::move(closure).Run();
  }

  Member<BroadcastChannel> channel_;
  HeapVector<Member<MessageEvent>> received_events_;
  Vector<BlinkCloneableMessage> sent_messages_;
  Vector<base::OnceClosure> on_next_update_;
  HeapMojoAssociatedReceiver<mojom::blink::BroadcastChannelClient,
                             BroadcastChannelTester>
      receiver_;
  HeapMojoAssociatedRemote<mojom::blink::BroadcastChannelClient> remote_;
};

namespace {

BlinkCloneableMessage MakeNullMessage() {
  BlinkCloneableMessage message;
  message.message = SerializedScriptValue::NullValue();
  message.sender_agent_cluster_id = base::UnguessableToken::Create();
  return message;
}

TEST(BroadcastChannelTest, DispatchMessageEvent) {
  test::TaskEnvironment task_environment;
  DummyPageHolder holder;
  ExecutionContext* execution_context = holder.GetFrame().DomWindow();
  auto* tester =
      MakeGarbageCollected<BroadcastChannelTester>(execution_context);

  base::RunLoop run_loop;
  tester->AwaitNextUpdate(run_loop.QuitClosure());
  tester->PostMessage(MakeNullMessage());
  run_loop.Run();

  ASSERT_EQ(tester->received_events().size(), 1u);
  EXPECT_EQ(tester->received_events()[0]->type(), event_type_names::kMessage);
}

TEST(BroadcastChannelTest, AgentClusterLockedMatch) {
  test::TaskEnvironment task_environment;
  DummyPageHolder holder;
  ExecutionContext* execution_context = holder.GetFrame().DomWindow();
  auto* tester =
      MakeGarbageCollected<BroadcastChannelTester>(execution_context);

  base::RunLoop run_loop;
  tester->AwaitNextUpdate(run_loop.QuitClosure());
  BlinkCloneableMessage message = MakeNullMessage();
  message.sender_agent_cluster_id = execution_context->GetAgentClusterID();
  message.locked_to_sender_agent_cluster = true;
  tester->PostMessage(std::move(message));
  run_loop.Run();

  ASSERT_EQ(tester->received_events().size(), 1u);
  EXPECT_EQ(tester->received_events()[0]->type(), event_type_names::kMessage);
}

TEST(BroadcastChannelTest, AgentClusterLockedMismatch) {
  test::TaskEnvironment task_environment;
  DummyPageHolder holder;
  ExecutionContext* execution_context = holder.GetFrame().DomWindow();
  auto* tester =
      MakeGarbageCollected<BroadcastChannelTester>(execution_context);

  base::RunLoop run_loop;
  tester->AwaitNextUpdate(run_loop.QuitClosure());
  BlinkCloneableMessage message = MakeNullMessage();
  message.locked_to_sender_agent_cluster = true;
  tester->PostMessage(std::move(message));
  run_loop.Run();

  ASSERT_EQ(tester->received_events().size(), 1u);
  EXPECT_EQ(tester->received_events()[0]->type(),
            event_type_names::kMessageerror);
}

TEST(BroadcastChannelTest, MessageCannotDeserialize) {
  test::TaskEnvironment task_environment;
  DummyPageHolder holder;
  LocalDOMWindow* window = holder.GetFrame().DomWindow();
  auto* tester = MakeGarbageCollected<BroadcastChannelTester>(window);

  SerializedScriptValue::ScopedOverrideCanDeserializeInForTesting
      override_can_deserialize_in(base::BindLambdaForTesting(
          [&](const SerializedScriptValue& value,
              ExecutionContext* execution_context, bool can_deserialize) {
            EXPECT_EQ(execution_context, window);
            EXPECT_TRUE(can_deserialize);
            return false;
          }));

  base::RunLoop run_loop;
  tester->AwaitNextUpdate(run_loop.QuitClosure());
  tester->PostMessage(MakeNullMessage());
  run_loop.Run();

  ASSERT_EQ(tester->received_events().size(), 1u);
  EXPECT_EQ(tester->received_events()[0]->type(),
            event_type_names::kMessageerror);
}

TEST(BroadcastChannelTest, OutgoingMessagesMarkedWithAgentClusterId) {
  test::TaskEnvironment task_environment;
  DummyPageHolder holder;
  ExecutionContext* execution_context = holder.GetFrame().DomWindow();
  ScriptState* script_state = ToScriptStateForMainWorld(&holder.GetFrame());
  auto* tester =
      MakeGarbageCollected<BroadcastChannelTester>(execution_context);

  base::RunLoop run_loop;
  tester->AwaitNextUpdate(run_loop.QuitClosure());
  {
    ScriptState::Scope scope(script_state);
    tester->channel()->postMessage(
        ScriptValue::CreateNull(script_state->GetIsolate()),
        ASSERT_NO_EXCEPTION);
  }
  run_loop.Run();

  ASSERT_EQ(tester->sent_messages().size(), 1u);
  EXPECT_EQ(tester->sent_messages()[0].sender_agent_cluster_id,
            execution_context->GetAgentClusterID());
  EXPECT_FALSE(tester->sent_messages()[0].locked_to_sender_agent_cluster);
}

// TODO(crbug.com/1413818): iOS doesn't support WebAssembly yet.
#if BUILDFLAG(IS_IOS)
#define MAYBE_OutgoingAgentClusterLockedMessage \
  DISABLED_OutgoingAgentClusterLockedMessage
#else
#define MAYBE_OutgoingAgentClusterLockedMessage \
  OutgoingAgentClusterLockedMessage
#endif

TEST(BroadcastChannelTest, MAYBE_OutgoingAgentClusterLockedMessage) {
  test::TaskEnvironment task_environment;
  DummyPageHolder holder;
  ExecutionContext* execution_context = holder.GetFrame().DomWindow();
  ScriptState* script_state = ToScriptStateForMainWorld(&holder.GetFrame());
  v8::Isolate* isolate = script_state->GetIsolate();
  auto* tester =
      MakeGarbageCollected<BroadcastChannelTester>(execution_context);

  base::RunLoop run_loop;
  tester->AwaitNextUpdate(run_loop.QuitClosure());
  {
    // WebAssembly modules are always agent cluster locked. This is a trivial
    // one with no functionality, just the minimal magic and version.
    static constexpr uint8_t kTrivialModuleBytes[] = {0x00, 0x61, 0x73, 0x6d,
                                                      0x01, 0x00, 0x00, 0x00};
    ScriptState::Scope scope(script_state);
    v8::Local<v8::WasmModuleObject> trivial_module =
        v8::WasmModuleObject::Compile(isolate, {std::data(kTrivialModuleBytes),
                                                std::size(kTrivialModuleBytes)})
            .ToLocalChecked();
    tester->channel()->postMessage(ScriptValue(isolate, trivial_module),
                                   ASSERT_NO_EXCEPTION);
  }
  run_loop.Run();

  ASSERT_EQ(tester->sent_messages().size(), 1u);
  EXPECT_EQ(tester->sent_messages()[0].sender_agent_cluster_id,
            execution_context->GetAgentClusterID());
  EXPECT_TRUE(tester->sent_messages()[0].locked_to_sender_agent_cluster);
}

}  // namespace
}  // namespace blink
