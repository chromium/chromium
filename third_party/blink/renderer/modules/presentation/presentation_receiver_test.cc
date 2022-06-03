// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_receiver.h"

#include <memory>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection_list.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

class MockEventListenerForPresentationReceiver : public NativeEventListener {
 public:
  MOCK_METHOD2(Invoke, void(ExecutionContext* executionContext, Event*));
};

class PresentationReceiverTest : public testing::Test {
 public:
  using ConnectionListProperty = PresentationReceiver::ConnectionListProperty;
  PresentationReceiverTest()
      : connection_info_(KURL("https://example.com"), "id") {}
  void AddConnectionavailableEventListener(EventListener*,
                                           const PresentationReceiver*);
  void VerifyConnectionListPropertyState(ConnectionListProperty::State,
                                         const PresentationReceiver*);
  void VerifyConnectionListSize(size_t expected_size,
                                const PresentationReceiver*);

 protected:
  void SetUp() override {
    controller_connection_receiver_ =
        controller_connection_.InitWithNewPipeAndPassReceiver();
    receiver_connection_receiver_ =
        receiver_connection_.InitWithNewPipeAndPassReceiver();
  }

  mojom::blink::PresentationInfo connection_info_;
  mojo::PendingReceiver<mojom::blink::PresentationConnection>
      controller_connection_receiver_;
  mojo::PendingRemote<mojom::blink::PresentationConnection>
      controller_connection_;
  mojo::PendingReceiver<mojom::blink::PresentationConnection>
      receiver_connection_receiver_;
  mojo::PendingRemote<mojom::blink::PresentationConnection>
      receiver_connection_;
};

void PresentationReceiverTest::AddConnectionavailableEventListener(
    EventListener* event_handler,
    const PresentationReceiver* receiver) {
  receiver->connection_list_->addEventListener(
      event_type_names::kConnectionavailable, event_handler);
}

void PresentationReceiverTest::VerifyConnectionListPropertyState(
    ConnectionListProperty::State expected_state,
    const PresentationReceiver* receiver) {
  EXPECT_EQ(expected_state, receiver->connection_list_property_->GetState());
}

void PresentationReceiverTest::VerifyConnectionListSize(
    size_t expected_size,
    const PresentationReceiver* receiver) {
  EXPECT_EQ(expected_size, receiver->connection_list_->connections_.size());
}

using testing::StrictMock;

TEST_F(PresentationReceiverTest, NoConnectionUnresolvedConnectionList) {
  V8TestingScope scope;
  auto* receiver =
      MakeGarbageCollected<PresentationReceiver>(&scope.GetWindow());

  auto* event_handler = MakeGarbageCollected<
      StrictMock<MockEventListenerForPresentationReceiver>>();
  AddConnectionavailableEventListener(event_handler, receiver);
  EXPECT_CALL(*event_handler, Invoke(testing::_, testing::_)).Times(0);

  receiver->connectionList(scope.GetScriptState());

  VerifyConnectionListPropertyState(ConnectionListProperty::kPending, receiver);
  VerifyConnectionListSize(0, receiver);
}

TEST_F(PresentationReceiverTest, OneConnectionResolvedConnectionListNoEvent) {
  V8TestingScope scope;
  auto* receiver =
      MakeGarbageCollected<PresentationReceiver>(&scope.GetWindow());

  auto* event_handler = MakeGarbageCollected<
      StrictMock<MockEventListenerForPresentationReceiver>>();
  AddConnectionavailableEventListener(event_handler, receiver);
  EXPECT_CALL(*event_handler, Invoke(testing::_, testing::_)).Times(0);

  receiver->connectionList(scope.GetScriptState());

  // Receive first connection.
  receiver->OnReceiverConnectionAvailable(
      connection_info_.Clone(), std::move(controller_connection_),
      std::move(receiver_connection_receiver_));

  VerifyConnectionListPropertyState(ConnectionListProperty::kResolved,
                                    receiver);
  VerifyConnectionListSize(1, receiver);
}

TEST_F(PresentationReceiverTest, TwoConnectionsFireOnconnectionavailableEvent) {
  V8TestingScope scope;
  auto* receiver =
      MakeGarbageCollected<PresentationReceiver>(&scope.GetWindow());

  StrictMock<MockEventListenerForPresentationReceiver>* event_handler =
      MakeGarbageCollected<
          StrictMock<MockEventListenerForPresentationReceiver>>();
  AddConnectionavailableEventListener(event_handler, receiver);
  EXPECT_CALL(*event_handler, Invoke(testing::_, testing::_)).Times(1);

  receiver->connectionList(scope.GetScriptState());

  // Receive first connection.
  receiver->OnReceiverConnectionAvailable(
      connection_info_.Clone(), std::move(controller_connection_),
      std::move(receiver_connection_receiver_));

  mojo::PendingRemote<mojom::blink::PresentationConnection>
      controller_connection_2_;
  mojo::PendingRemote<mojom::blink::PresentationConnection>
      receiver_connection_2_;
  mojo::PendingReceiver<mojom::blink::PresentationConnection>
      controller_connection_receiver_2 =
          controller_connection_2_.InitWithNewPipeAndPassReceiver();
  mojo::PendingReceiver<mojom::blink::PresentationConnection>
      receiver_connection_receiver_2 =
          receiver_connection_2_.InitWithNewPipeAndPassReceiver();

  // Receive second connection.
  receiver->OnReceiverConnectionAvailable(
      connection_info_.Clone(), std::move(controller_connection_2_),
      std::move(receiver_connection_receiver_2));

  VerifyConnectionListSize(2, receiver);
}

TEST_F(PresentationReceiverTest, TwoConnectionsNoEvent) {
  V8TestingScope scope;
  auto* receiver =
      MakeGarbageCollected<PresentationReceiver>(&scope.GetWindow());

  StrictMock<MockEventListenerForPresentationReceiver>* event_handler =
      MakeGarbageCollected<
          StrictMock<MockEventListenerForPresentationReceiver>>();
  AddConnectionavailableEventListener(event_handler, receiver);
  EXPECT_CALL(*event_handler, Invoke(testing::_, testing::_)).Times(0);

  // Receive first connection.
  receiver->OnReceiverConnectionAvailable(
      connection_info_.Clone(), std::move(controller_connection_),
      std::move(receiver_connection_receiver_));

  mojo::PendingRemote<mojom::blink::PresentationConnection>
      controller_connection_2_;
  mojo::PendingRemote<mojom::blink::PresentationConnection>
      receiver_connection_2_;
  mojo::PendingReceiver<mojom::blink::PresentationConnection>
      controller_connection_receiver_2 =
          controller_connection_2_.InitWithNewPipeAndPassReceiver();
  mojo::PendingReceiver<mojom::blink::PresentationConnection>
      receiver_connection_receiver_2 =
          receiver_connection_2_.InitWithNewPipeAndPassReceiver();

  // Receive second connection.
  receiver->OnReceiverConnectionAvailable(
      connection_info_.Clone(), std::move(controller_connection_2_),
      std::move(receiver_connection_receiver_2));

  receiver->connectionList(scope.GetScriptState());
  VerifyConnectionListPropertyState(ConnectionListProperty::kResolved,
                                    receiver);
  VerifyConnectionListSize(2, receiver);
}

}  // namespace blink
