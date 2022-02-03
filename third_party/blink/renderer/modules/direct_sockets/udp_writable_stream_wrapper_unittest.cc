// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/udp_writable_stream_wrapper.h"

#include "base/bind.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_udp_message.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_socket_mojo_remote.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock-matchers.h"

namespace blink {

namespace {

class FakeDirectUDPSocket : public blink::mojom::blink::DirectUDPSocket {
 public:
  void Send(base::span<const uint8_t> data, SendCallback callback) override {
    data_.Append(data.data(), static_cast<uint32_t>(data.size_bytes()));
    std::move(callback).Run(net::Error::OK);
  }

  void ReceiveMore(uint32_t num_additional_datagrams) override { NOTREACHED(); }

  void Close() override { NOTREACHED(); }

  const Vector<uint8_t>& GetReceivedData() const { return data_; }

 private:
  Vector<uint8_t> data_;
};

class StreamCreator {
  STACK_ALLOCATED();

 public:
  explicit StreamCreator(const V8TestingScope& scope,
                         FakeDirectUDPSocket* fake_udp_socket)
      : scope_(scope), receiver_{fake_udp_socket} {}

  ~StreamCreator() { test::RunPendingTasks(); }

  UDPWritableStreamWrapper* Create() {
    auto* udp_socket =
        MakeGarbageCollected<UDPSocketMojoRemote>(scope_.GetExecutionContext());
    udp_socket->get().Bind(
        receiver_.BindNewPipeAndPassRemote(),
        scope_.GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));

    auto* script_state = scope_.GetScriptState();
    auto* udp_writable_stream_wrapper =
        MakeGarbageCollected<UDPWritableStreamWrapper>(script_state,
                                                       udp_socket);
    return udp_writable_stream_wrapper;
  }

 private:
  const V8TestingScope& scope_;
  mojo::Receiver<blink::mojom::blink::DirectUDPSocket> receiver_;
};

TEST(UDPWritableStreamWrapperTest, Create) {
  V8TestingScope scope;
  FakeDirectUDPSocket fake_udp_socket;
  StreamCreator stream_creator{scope, &fake_udp_socket};
  auto* udp_writable_stream_wrapper = stream_creator.Create();
  EXPECT_TRUE(udp_writable_stream_wrapper->Writable());
}

TEST(UDPWritableStreamWrapperTest, WriteUdpMessage) {
  V8TestingScope scope;
  FakeDirectUDPSocket fake_udp_socket;
  StreamCreator stream_creator{scope, &fake_udp_socket};

  auto* udp_writable_stream_wrapper = stream_creator.Create();
  auto* script_state = scope.GetScriptState();

  auto* writer = udp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);

  auto* chunk = DOMArrayBuffer::Create("A", 1);
  auto* message = UDPMessage::Create();
  message->setData(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(chunk));

  ScriptPromise result =
      writer->write(script_state, ScriptValue::From(script_state, message),
                    ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();

  ASSERT_TRUE(tester.IsFulfilled());

  EXPECT_THAT(fake_udp_socket.GetReceivedData(), ::testing::ElementsAre('A'));
}

TEST(UDPWritableStreamWrapperTest, WriteUdpMessageFromTypedArray) {
  V8TestingScope scope;
  FakeDirectUDPSocket fake_udp_socket;
  StreamCreator stream_creator{scope, &fake_udp_socket};

  auto* udp_writable_stream_wrapper = stream_creator.Create();
  auto* script_state = scope.GetScriptState();

  auto* writer = udp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);

  auto* buffer = DOMArrayBuffer::Create("ABC", 3);
  auto* chunk = DOMUint8Array::Create(buffer, 0, 3);

  auto* message = UDPMessage::Create();
  message->setData(MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
      NotShared<DOMUint8Array>(chunk)));

  ScriptPromise result =
      writer->write(script_state, ScriptValue::From(script_state, message),
                    ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();

  ASSERT_TRUE(tester.IsFulfilled());

  EXPECT_THAT(fake_udp_socket.GetReceivedData(),
              ::testing::ElementsAre('A', 'B', 'C'));
}

TEST(UDPWritableStreamWrapperTest, WriteUdpMessageWithEmptyDataField) {
  V8TestingScope scope;
  FakeDirectUDPSocket fake_udp_socket;
  StreamCreator stream_creator{scope, &fake_udp_socket};

  auto* udp_writable_stream_wrapper = stream_creator.Create();
  auto* script_state = scope.GetScriptState();

  auto* writer = udp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);

  // Create empty DOMArrayBuffer.
  auto* chunk = DOMArrayBuffer::Create(/*num_elements=*/static_cast<size_t>(0),
                                       /*element_byte_size=*/1);
  auto* message = UDPMessage::Create();
  message->setData(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(chunk));

  ScriptPromise result =
      writer->write(script_state, ScriptValue::From(script_state, message),
                    ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();

  ASSERT_TRUE(tester.IsFulfilled());

  // Nothing should have been written from the empty DOMArrayBuffer.
  EXPECT_THAT(fake_udp_socket.GetReceivedData(), ::testing::ElementsAre());
}

TEST(UDPWritableStreamWrapperTest, WriteUdpMessageWithoutDataField) {
  V8TestingScope scope;
  FakeDirectUDPSocket fake_udp_socket;
  StreamCreator stream_creator{scope, &fake_udp_socket};
  auto* udp_writable_stream_wrapper = stream_creator.Create();
  auto* script_state = scope.GetScriptState();

  auto* writer = udp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);

  // Create empty message (without 'data' field).
  auto* message = UDPMessage::Create();

  ScriptPromise result =
      writer->write(script_state, ScriptValue::From(script_state, message),
                    ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();

  // Should be rejected due to missing 'data' field.
  ASSERT_TRUE(tester.IsRejected());

  DOMException* exception = V8DOMException::ToImplWithTypeCheck(
      scope.GetIsolate(), tester.Value().V8Value());

  ASSERT_TRUE(exception);
  ASSERT_EQ(exception->name(), "DataError");
  ASSERT_EQ(exception->message(),
            "Failed to execute 'write' on 'UnderlyingSinkBase': UDPMessage: "
            "missing 'data' field.");
}

TEST(UDPWritableStreamWrapperTest, WriteAfterFinishedWrite) {
  V8TestingScope scope;
  FakeDirectUDPSocket fake_udp_socket;
  StreamCreator stream_creator{scope, &fake_udp_socket};
  auto* udp_writable_stream_wrapper = stream_creator.Create();
  auto* script_state = scope.GetScriptState();

  auto* writer = udp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);

  for (const auto* value : {"A", "B"}) {
    auto* chunk = DOMArrayBuffer::Create(value, 1);
    auto* message = UDPMessage::Create();
    message->setData(
        MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(chunk));

    ScriptPromise result =
        writer->write(script_state, ScriptValue::From(script_state, message),
                      ASSERT_NO_EXCEPTION);

    ScriptPromiseTester tester(script_state, result);
    tester.WaitUntilSettled();

    ASSERT_TRUE(tester.IsFulfilled());
  }

  EXPECT_THAT(fake_udp_socket.GetReceivedData(),
              ::testing::ElementsAre('A', 'B'));
}

TEST(UDPWritableStreamWrapperTest, WriteAfterClose) {
  V8TestingScope scope;
  FakeDirectUDPSocket fake_udp_socket;
  StreamCreator stream_creator{scope, &fake_udp_socket};
  auto* udp_writable_stream_wrapper = stream_creator.Create();
  auto* script_state = scope.GetScriptState();

  auto* writer = udp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);

  auto* chunk = DOMArrayBuffer::Create("A", 1);
  auto* message = UDPMessage::Create();
  message->setData(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(chunk));

  ScriptPromise write_result =
      writer->write(script_state, ScriptValue::From(script_state, message),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester write_tester(script_state, write_result);
  write_tester.WaitUntilSettled();

  ASSERT_TRUE(write_tester.IsFulfilled());

  ScriptPromise close_result = writer->close(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester close_tester(script_state, close_result);
  close_tester.WaitUntilSettled();

  ASSERT_TRUE(write_tester.IsFulfilled());

  ScriptPromise write_after_close_result =
      writer->write(script_state, ScriptValue::From(script_state, message),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester write_after_close_tester(script_state,
                                               write_after_close_result);
  write_after_close_tester.WaitUntilSettled();

  ASSERT_TRUE(write_after_close_tester.IsRejected());
}

}  // namespace

}  // namespace blink
