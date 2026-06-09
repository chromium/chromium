// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_connection_callbacks.h"

#include <utility>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection.h"
#include "third_party/blink/renderer/modules/presentation/presentation_request.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

constexpr char kPresentationUrl[] = "https://example.com";
constexpr char kPresentationId[] = "xyzzy";

namespace blink {

using mojom::blink::PresentationConnectionResult;
using mojom::blink::PresentationConnectionResultPtr;
using mojom::blink::PresentationConnectionState;
using mojom::blink::PresentationError;
using mojom::blink::PresentationErrorType;
using mojom::blink::PresentationInfo;
using mojom::blink::PresentationInfoPtr;

namespace {

static PresentationRequest* MakeRequest(V8TestingScope* scope) {
  PresentationRequest* request =
      PresentationRequest::Create(scope->GetExecutionContext(),
                                  kPresentationUrl, scope->GetExceptionState());
  EXPECT_FALSE(scope->GetExceptionState().HadException());
  return request;
}

}  // namespace

TEST(PresentationConnectionCallbacksTest, HandleSuccess) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationConnection>>(
          scope.GetScriptState());
  ScriptPromiseTester promise_tester(scope.GetScriptState(),
                                     resolver->Promise());

  PresentationConnectionCallbacks callbacks(resolver, MakeRequest(&scope));

  // No connection currently exists.
  EXPECT_FALSE(callbacks.connection_);

  mojo::PendingRemote<mojom::blink::PresentationConnection> connection_remote;
  mojo::PendingReceiver<mojom::blink::PresentationConnection>
      connection_receiver = connection_remote.InitWithNewPipeAndPassReceiver();
  PresentationConnectionResultPtr result = PresentationConnectionResult::New(
      PresentationInfo::New(url_test_helpers::ToKURL(kPresentationUrl),
                            kPresentationId),
      std::move(connection_remote), std::move(connection_receiver));

  callbacks.HandlePresentationResponse(std::move(result), nullptr);

  // New connection was created.
  ControllerPresentationConnection* connection = callbacks.connection_.Get();
  ASSERT_TRUE(connection);
  EXPECT_EQ(connection->GetState(), PresentationConnectionState::CONNECTING);

  // Connection must be closed before the next connection test.
  connection->close();

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsFulfilled());
}

TEST(PresentationConnectionCallbacksTest, HandleReconnect) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  PresentationInfoPtr info = PresentationInfo::New(
      url_test_helpers::ToKURL(kPresentationUrl), kPresentationId);
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationConnection>>(
          scope.GetScriptState());
  ScriptPromiseTester promise_tester(scope.GetScriptState(),
                                     resolver->Promise());

  auto* connection = ControllerPresentationConnection::Create(
      scope.GetExecutionContext(), *info, MakeRequest(&scope));
  // Connection must be closed for reconnection to succeed.
  connection->close();

  PresentationConnectionCallbacks callbacks(resolver, connection);

  mojo::PendingRemote<mojom::blink::PresentationConnection> connection_remote;
  mojo::PendingReceiver<mojom::blink::PresentationConnection>
      connection_receiver = connection_remote.InitWithNewPipeAndPassReceiver();
  PresentationConnectionResultPtr result = PresentationConnectionResult::New(
      std::move(info), std::move(connection_remote),
      std::move(connection_receiver));

  callbacks.HandlePresentationResponse(std::move(result), nullptr);

  // Previous connection was returned.
  ControllerPresentationConnection* new_connection =
      callbacks.connection_.Get();
  EXPECT_EQ(connection, new_connection);
  EXPECT_EQ(new_connection->GetState(),
            PresentationConnectionState::CONNECTING);

  // Connection must be closed before the next connection test.
  connection->close();

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsFulfilled());
}

TEST(PresentationConnectionCallbacksTest, HandleError) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PresentationConnection>>(
          scope.GetScriptState());
  ScriptPromiseTester promise_tester(scope.GetScriptState(),
                                     resolver->Promise());

  PresentationConnectionCallbacks callbacks(resolver, MakeRequest(&scope));

  // No connection currently exists.
  EXPECT_FALSE(callbacks.connection_);

  callbacks.HandlePresentationResponse(
      nullptr,
      PresentationError::New(PresentationErrorType::NO_PRESENTATION_FOUND,
                             "Something bad happened"));

  // No connection was created.
  EXPECT_FALSE(callbacks.connection_);

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
}

class MockPresentationConnectionReceiver
    : public mojom::blink::PresentationConnection {
 public:
  explicit MockPresentationConnectionReceiver(
      mojo::PendingReceiver<mojom::blink::PresentationConnection> receiver)
      : receiver_(this, std::move(receiver)) {}

  void OnMessage(
      mojom::blink::PresentationConnectionMessagePtr message) override {
    messages_.push_back(std::move(message));
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void DidChangeState(
      mojom::blink::PresentationConnectionState state) override {}
  void DidClose(
      mojom::blink::PresentationConnectionCloseReason reason) override {}

  void WaitForMessage() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  std::vector<mojom::blink::PresentationConnectionMessagePtr> messages_;
  mojo::Receiver<mojom::blink::PresentationConnection> receiver_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

TEST(PresentationConnectionTest, SendArrayBufferViewSendsOnlySlice) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  PresentationInfoPtr info = PresentationInfo::New(
      url_test_helpers::ToKURL(kPresentationUrl), kPresentationId);

  // Create connection.
  auto* connection = ControllerPresentationConnection::Create(
      scope.GetExecutionContext(), *info, MakeRequest(&scope));

  // Set up Mojo pipes.
  mojo::PendingRemote<mojom::blink::PresentationConnection> connection_remote;
  mojo::PendingReceiver<mojom::blink::PresentationConnection>
      connection_receiver = connection_remote.InitWithNewPipeAndPassReceiver();

  mojo::PendingRemote<mojom::blink::PresentationConnection> target_remote;
  mojo::PendingReceiver<mojom::blink::PresentationConnection> target_receiver =
      target_remote.InitWithNewPipeAndPassReceiver();

  // Bind the mock receiver to target_receiver (this will receive messages sent
  // by Blink).
  MockPresentationConnectionReceiver mock_receiver(std::move(target_receiver));

  // Initialize the connection.
  connection->Init(std::move(target_remote), std::move(connection_receiver));
  connection->DidChangeState(
      mojom::blink::PresentationConnectionState::CONNECTED);

  // Create a backing ArrayBuffer of 10 bytes.
  DOMArrayBuffer* backing = DOMArrayBuffer::Create(10, 1);
  base::span<uint8_t> backing_span = backing->ByteSpan();
  std::fill(backing_span.begin(), backing_span.end(), 0x41);

  // Create a view of 4 bytes starting at offset 3, filled with 'B' (0x42).
  // The view slice: indices [3, 4, 5, 6] should contain 'B'.
  // Backing buffer: [A, A, A, B, B, B, B, A, A, A]
  base::span<uint8_t> view_span = backing_span.subspan(3u, 4u);
  std::fill(view_span.begin(), view_span.end(), 0x42);

  DOMArrayBufferView* view = DOMDataView::Create(backing, 3u, 4u);

  // Send the view.
  connection->send(NotShared<DOMArrayBufferView>(view),
                   scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  // Wait for Mojo message.
  mock_receiver.WaitForMessage();

  // Verify message.
  ASSERT_EQ(mock_receiver.messages_.size(), 1u);
  const auto& msg = mock_receiver.messages_[0];
  ASSERT_TRUE(msg->is_data());

  const auto& sent_data = msg->get_data();
  // We expect only 4 bytes (the view slice), not the entire 10-byte backing
  // buffer!
  EXPECT_EQ(sent_data.size(), 4u);

  // Verify contents: all bytes in the sent data must be 'B' (0x42).
  for (unsigned char i : sent_data) {
    EXPECT_EQ(i, 0x42);
  }

  connection->close();
}

}  // namespace blink
