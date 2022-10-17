// Copyright 2022 The Chromium Authors
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
#include "third_party/blink/renderer/platform/heap/persistent.h"
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

class StreamCreator : public GarbageCollected<StreamCreator> {
 public:
  StreamCreator()
      : fake_udp_socket_{std::make_unique<FakeDirectUDPSocket>()},
        receiver_{fake_udp_socket_.get()} {}

  explicit StreamCreator(std::unique_ptr<FakeDirectUDPSocket> socket)
      : fake_udp_socket_(std::move(socket)),
        receiver_{fake_udp_socket_.get()} {}

  ~StreamCreator() = default;

  UDPWritableStreamWrapper* Create(const V8TestingScope& scope) {
    auto* udp_socket =
        MakeGarbageCollected<UDPSocketMojoRemote>(scope.GetExecutionContext());
    udp_socket->get().Bind(
        receiver_.BindNewPipeAndPassRemote(),
        scope.GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));

    auto* script_state = scope.GetScriptState();
    stream_wrapper_ = MakeGarbageCollected<UDPWritableStreamWrapper>(
        script_state,
        WTF::BindOnce(&StreamCreator::Close, WrapWeakPersistent(this)),
        udp_socket);
    return stream_wrapper_;
  }

  void Trace(Visitor* visitor) const { visitor->Trace(stream_wrapper_); }

  FakeDirectUDPSocket* fake_udp_socket() { return fake_udp_socket_.get(); }

  bool CloseCalledWith(bool error) { return close_called_with_ == error; }

  void Cleanup() {
    fake_udp_socket_.reset();
    receiver_.reset();
  }

 private:
  void Close(ScriptValue exception) {
    close_called_with_ = !exception.IsEmpty();
  }

  absl::optional<bool> close_called_with_;
  std::unique_ptr<FakeDirectUDPSocket> fake_udp_socket_;
  mojo::Receiver<blink::mojom::blink::DirectUDPSocket> receiver_;
  Member<UDPWritableStreamWrapper> stream_wrapper_;
};

class ScopedStreamCreator {
 public:
  explicit ScopedStreamCreator(StreamCreator* stream_creator)
      : stream_creator_(stream_creator) {}

  ~ScopedStreamCreator() { stream_creator_->Cleanup(); }

  StreamCreator* operator->() const { return stream_creator_; }

 private:
  Persistent<StreamCreator> stream_creator_;
};

TEST(UDPWritableStreamWrapperTest, Create) {
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* udp_writable_stream_wrapper = stream_creator->Create(scope);

  EXPECT_TRUE(udp_writable_stream_wrapper->Writable());
}

TEST(UDPWritableStreamWrapperTest, WriteUdpMessage) {
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* udp_writable_stream_wrapper = stream_creator->Create(scope);

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

  auto* fake_udp_socket = stream_creator->fake_udp_socket();
  EXPECT_THAT(fake_udp_socket->GetReceivedData(), ::testing::ElementsAre('A'));
}

TEST(UDPWritableStreamWrapperTest, WriteUdpMessageFromTypedArray) {
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* udp_writable_stream_wrapper = stream_creator->Create(scope);

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

  auto* fake_udp_socket = stream_creator->fake_udp_socket();
  EXPECT_THAT(fake_udp_socket->GetReceivedData(),
              ::testing::ElementsAre('A', 'B', 'C'));
}

TEST(UDPWritableStreamWrapperTest, WriteUdpMessageWithEmptyDataField) {
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* udp_writable_stream_wrapper = stream_creator->Create(scope);

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
  auto* fake_udp_socket = stream_creator->fake_udp_socket();
  EXPECT_THAT(fake_udp_socket->GetReceivedData(), ::testing::ElementsAre());
}

TEST(UDPWritableStreamWrapperTest, WriteUdpMessageWithoutDataField) {
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* udp_writable_stream_wrapper = stream_creator->Create(scope);

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
  ASSERT_TRUE(exception->message().Contains("missing 'data' field"));
}

TEST(UDPWritableStreamWrapperTest, WriteAfterFinishedWrite) {
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* udp_writable_stream_wrapper = stream_creator->Create(scope);

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

  auto* fake_udp_socket = stream_creator->fake_udp_socket();
  EXPECT_THAT(fake_udp_socket->GetReceivedData(),
              ::testing::ElementsAre('A', 'B'));
}

TEST(UDPWritableStreamWrapperTest, WriteAfterClose) {
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>());
  auto* udp_writable_stream_wrapper = stream_creator->Create(scope);

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

  ASSERT_EQ(udp_writable_stream_wrapper->GetState(),
            StreamWrapper::State::kClosed);

  ScriptPromise write_after_close_result =
      writer->write(script_state, ScriptValue::From(script_state, message),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester write_after_close_tester(script_state,
                                               write_after_close_result);
  write_after_close_tester.WaitUntilSettled();

  ASSERT_TRUE(write_after_close_tester.IsRejected());
}

TEST(UDPWritableStreamWrapperTest, WriteFailed) {
  class FailingFakeDirectUDPSocket : public FakeDirectUDPSocket {
   public:
    void Send(base::span<const uint8_t> data, SendCallback callback) override {
      std::move(callback).Run(net::ERR_UNEXPECTED);
    }
  };

  V8TestingScope scope;

  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>(
      std::make_unique<FailingFakeDirectUDPSocket>()));
  auto* udp_writable_stream_wrapper = stream_creator->Create(scope);

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

  ASSERT_TRUE(write_tester.IsRejected());
  ASSERT_EQ(udp_writable_stream_wrapper->GetState(),
            StreamWrapper::State::kAborted);

  ASSERT_TRUE(stream_creator->CloseCalledWith(/*error=*/true));
}

}  // namespace

}  // namespace blink
