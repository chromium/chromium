// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/messaging/message_port.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/wait_for_event.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

BlinkTransferableMessage MakeNullMessage() {
  BlinkTransferableMessage message;
  message.message = SerializedScriptValue::NullValue();
  message.sender_agent_cluster_id = base::UnguessableToken::Create();
  return message;
}

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

}  // namespace
}  // namespace blink
