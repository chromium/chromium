// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/udp_writable_stream_wrapper.h"

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/restricted_udp_socket.mojom-blink.h"
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
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/gc_plugin.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock-matchers.h"

namespace blink {

namespace {

class FakeRestrictedUDPSocket
    : public GarbageCollected<FakeRestrictedUDPSocket>,
      public network::mojom::blink::RestrictedUDPSocket {
 public:
  void Send(base::span<const uint8_t> data, SendCallback callback) override {
    data_.AppendSpan(data);
    std::move(callback).Run(net::Error::OK);
  }

  void SendTo(base::span<const uint8_t> data,
              const net::HostPortPair& dest_addr,
              net::DnsQueryType dns_query_type,
              SendToCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void ReceiveMore(uint32_t num_additional_datagrams) override {
    NOTREACHED_IN_MIGRATION();
  }

  const Vector<uint8_t>& GetReceivedData() const { return data_; }
  void Trace(cppgc::Visitor* visitor) const {}

 private:
  Vector<uint8_t> data_;
};

class StreamCreator : public GarbageCollected<StreamCreator> {
 public:
  explicit StreamCreator(const V8TestingScope& scope)
      : StreamCreator(scope, MakeGarbageCollected<FakeRestrictedUDPSocket>()) {}

  StreamCreator(const V8TestingScope& scope, FakeRestrictedUDPSocket* socket)
      : fake_udp_socket_(socket),
        receiver_{fake_udp_socket_.Get(), scope.GetExecutionContext()} {}

  ~StreamCreator() = default;

  UDPWritableStreamWrapper* Create(const V8TestingScope& scope) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        scope.GetExecutionContext()->GetTaskRunner(TaskType::kNetworking);
    auto* udp_socket =
        MakeGarbageCollected<UDPSocketMojoRemote>(scope.GetExecutionContext());
    udp_socket->get().Bind(receiver_.BindNewPipeAndPassRemote(task_runner),
                           task_runner);

    auto* script_state = scope.GetScriptState();
    stream_wrapper_ = MakeGarbageCollected<UDPWritableStreamWrapper>(
        script_state,
        WTF::BindOnce(&StreamCreator::Close, WrapWeakPersistent(this)),
        udp_socket, network::mojom::RestrictedUDPSocketMode::CONNECTED);
    return stream_wrapper_.Get();
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(fake_udp_socket_);
    visitor->Trace(receiver_);
    visitor->Trace(stream_wrapper_);
  }

  FakeRestrictedUDPSocket* fake_udp_socket() { return fake_udp_socket_.Get(); }

  bool CloseCalledWith(bool error) { return close_called_with_ == error; }

  void Cleanup() { receiver_.reset(); }

 private:
  void Close(ScriptValue exception) {
    close_called_with_ = !exception.IsEmpty();
  }

  std::optional<bool> close_called_with_;
  Member<FakeRestrictedUDPSocket> fake_udp_socket_;
  HeapMojoReceiver<network::mojom::blink::RestrictedUDPSocket,
                   FakeRestrictedUDPSocket>
      receiver_;
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
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(
      MakeGarbageCollected<StreamCreator>(scope));
  auto* udp_writable_stream_wrapper = stream_creator->Create(scope);

  EXPECT_TRUE(udp_writable_stream_wrapper->Writable());
}

TEST(UDPWritableStreamWrapperTest, WriteUdpMessage) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(
      MakeGarbageCollected<StreamCreator>(scope));
  auto* udp_writable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();

  auto* writer = udp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);

  auto* chunk = DOMArrayBuffer::Create(base::byte_span_from_cstring("A"));
  auto* message = UDPMessage::Create();
  message->setData(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(chunk));

  ScriptPromiseUntyped result =
      writer->write(script_state, ScriptValue::From(script_state, message),
                    ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();

  ASSERT_TRUE(tester.IsFulfilled());

  auto* fake_udp_socket = stream_creator->fake_udp_socket();
  EXPECT_THAT(fake_udp_socket->GetReceivedData(), ::testing::ElementsAre('A'));
}

TEST(UDPWritableStreamWrapperTest, WriteUdpMessageFromTypedArray) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(
      MakeGarbageCollected<StreamCreator>(scope));
  auto* udp_writable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();

  auto* writer = udp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);

  auto* buffer = DOMArrayBuffer::Create(base::byte_span_from_cstring("ABC"));
  auto* chunk = DOMUint8Array::Create(buffer, 0, 3);

  auto* message = UDPMessage::Create();
  message->setData(MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
      NotShared<DOMUint8Array>(chunk)));

  ScriptPromiseUntyped result =
      writer->write(script_state, ScriptValue::From(script_state, message),
                    ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();

  ASSERT_TRUE(tester.IsFulfilled());

  auto* fake_udp_socket = stream_creator->fake_udp_socket();
  EXPECT_THAT(fake_udp_socket->GetReceivedData(),
              ::testing::ElementsAre('A', 'B', 'C'));
}

TEST(UDPWritableStreamWrapperTest, WriteUdpMessageWithEmptyDataFieldFails) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(
      MakeGarbageCollected<StreamCreator>(scope));
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

  ScriptPromiseUntyped result =
      writer->write(script_state, ScriptValue::From(script_state, message),
                    ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester(script_state, result);
  tester.WaitUntilSettled();

  ASSERT_TRUE(tester.IsRejected());
}

TEST(UDPWritableStreamWrapperTest, WriteAfterFinishedWrite) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(
      MakeGarbageCollected<StreamCreator>(scope));
  auto* udp_writable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();

  auto* writer = udp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);

  for (const std::string_view value : {"A", "B"}) {
    auto* chunk = DOMArrayBuffer::Create(base::as_byte_span(value));
    auto* message = UDPMessage::Create();
    message->setData(
        MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(chunk));

    ScriptPromiseUntyped result =
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
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(
      MakeGarbageCollected<StreamCreator>(scope));
  auto* udp_writable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();

  auto* writer = udp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);

  auto* chunk = DOMArrayBuffer::Create(base::byte_span_from_cstring("A"));
  auto* message = UDPMessage::Create();
  message->setData(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(chunk));

  ScriptPromiseUntyped write_result =
      writer->write(script_state, ScriptValue::From(script_state, message),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester write_tester(script_state, write_result);
  write_tester.WaitUntilSettled();

  ASSERT_TRUE(write_tester.IsFulfilled());

  ScriptPromiseUntyped close_result =
      writer->close(script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester close_tester(script_state, close_result);
  close_tester.WaitUntilSettled();

  ASSERT_TRUE(write_tester.IsFulfilled());

  ASSERT_EQ(udp_writable_stream_wrapper->GetState(),
            StreamWrapper::State::kClosed);

  ScriptPromiseUntyped write_after_close_result =
      writer->write(script_state, ScriptValue::From(script_state, message),
                    ASSERT_NO_EXCEPTION);
  ScriptPromiseTester write_after_close_tester(script_state,
                                               write_after_close_result);
  write_after_close_tester.WaitUntilSettled();

  ASSERT_TRUE(write_after_close_tester.IsRejected());
}

TEST(UDPWritableStreamWrapperTest, WriteFailed) {
  class FailingFakeRestrictedUDPSocket : public FakeRestrictedUDPSocket {
   public:
    void Send(base::span<const uint8_t> data, SendCallback callback) override {
      std::move(callback).Run(net::ERR_UNEXPECTED);
    }
  };

  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(MakeGarbageCollected<StreamCreator>(
      scope, MakeGarbageCollected<FailingFakeRestrictedUDPSocket>()));
  auto* udp_writable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();
  auto* writer = udp_writable_stream_wrapper->Writable()->getWriter(
      script_state, ASSERT_NO_EXCEPTION);

  auto* chunk = DOMArrayBuffer::Create(base::byte_span_from_cstring("A"));
  auto* message = UDPMessage::Create();
  message->setData(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(chunk));

  ScriptPromiseUntyped write_result =
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
