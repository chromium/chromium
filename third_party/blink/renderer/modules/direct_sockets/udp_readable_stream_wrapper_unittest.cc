// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/udp_readable_stream_wrapper.h"

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/restricted_udp_socket.mojom-blink.h"
#include "services/network/public/mojom/udp_socket.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_udp_message.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_writable_stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/gc_plugin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {
namespace {

class FakeRestrictedUDPSocket final
    : public GarbageCollected<FakeRestrictedUDPSocket>,
      public network::mojom::blink::RestrictedUDPSocket {
 public:
  explicit FakeRestrictedUDPSocket(ContextLifecycleNotifier* notifier)
      : remote_(notifier) {}
  void Send(base::span<const uint8_t> data, SendCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void SendTo(base::span<const uint8_t> data,
              const net::HostPortPair& dest_addr,
              net::DnsQueryType dns_query_type,
              SendToCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void ReceiveMore(uint32_t num_additional_datagrams) override {
    num_requested_datagrams += num_additional_datagrams;
  }

  void ProvideRequestedDatagrams() {
    DCHECK(remote_.is_bound());
    while (num_requested_datagrams > 0) {
      remote_->OnReceived(net::OK,
                          net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0U},
                          datagram_.Span8());
      num_requested_datagrams--;
    }
  }

  void Bind(mojo::PendingRemote<network::mojom::blink::UDPSocketListener>
                pending_remote,
            scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    remote_.Bind(std::move(pending_remote), task_runner);
  }

  const String& GetTestingDatagram() const { return datagram_; }
  void SetTestingDatagram(String datagram) { datagram_ = std::move(datagram); }

  void Trace(Visitor* visitor) const { visitor->Trace(remote_); }

 private:
  HeapMojoRemote<network::mojom::blink::UDPSocketListener> remote_;
  uint32_t num_requested_datagrams = 0;
  String datagram_{"abcde"};
};

class StreamCreator : public GarbageCollected<StreamCreator> {
 public:
  explicit StreamCreator(const V8TestingScope& scope)
      : fake_udp_socket_(MakeGarbageCollected<FakeRestrictedUDPSocket>(
            scope.GetExecutionContext())),
        receiver_(fake_udp_socket_.Get(), scope.GetExecutionContext()) {}

  ~StreamCreator() = default;

  UDPReadableStreamWrapper* Create(V8TestingScope& scope) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        scope.GetExecutionContext()->GetTaskRunner(TaskType::kNetworking);
    auto* udp_socket =
        MakeGarbageCollected<UDPSocketMojoRemote>(scope.GetExecutionContext());
    udp_socket->get().Bind(receiver_.BindNewPipeAndPassRemote(task_runner),
                           task_runner);

    mojo::PendingReceiver<network::mojom::blink::UDPSocketListener> receiver;
    fake_udp_socket_->Bind(receiver.InitWithNewPipeAndPassRemote(),
                           task_runner);

    auto* script_state = scope.GetScriptState();
    stream_wrapper_ = MakeGarbageCollected<UDPReadableStreamWrapper>(
        script_state, base::DoNothing(), udp_socket, std::move(receiver));

    // Ensure that udp_socket->ReceiveMore(...) call from
    // UDPReadableStreamWrapper constructor completes.
    scope.PerformMicrotaskCheckpoint();
    test::RunPendingTasks();

    return stream_wrapper_.Get();
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(fake_udp_socket_);
    visitor->Trace(stream_wrapper_);
    visitor->Trace(receiver_);
  }

  FakeRestrictedUDPSocket& fake_udp_socket() { return *fake_udp_socket_; }

  void Cleanup() { receiver_.reset(); }

 private:
  Member<FakeRestrictedUDPSocket> fake_udp_socket_;
  Member<UDPReadableStreamWrapper> stream_wrapper_;

  HeapMojoReceiver<network::mojom::blink::RestrictedUDPSocket,
                   FakeRestrictedUDPSocket>
      receiver_;
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

std::pair<UDPMessage*, bool> UnpackPromiseResult(const V8TestingScope& scope,
                                                 v8::Local<v8::Value> result) {
  // js call looks like this:
  // let { value, done } = await reader.read();
  // So we have to unpack the iterator first.
  EXPECT_TRUE(result->IsObject());
  v8::Local<v8::Value> udp_message_packed;
  bool done = false;
  EXPECT_TRUE(V8UnpackIterationResult(scope.GetScriptState(),
                                      result.As<v8::Object>(),
                                      &udp_message_packed, &done));
  if (done) {
    return {nullptr, true};
  }
  auto* message = NativeValueTraits<UDPMessage>::NativeValue(
      scope.GetIsolate(), udp_message_packed, ASSERT_NO_EXCEPTION);

  return {message, false};
}

String UDPMessageDataToString(const UDPMessage* message) {
  DOMArrayPiece array_piece{message->data()};
  return String{static_cast<const uint8_t*>(array_piece.Bytes()),
                static_cast<wtf_size_t>(array_piece.ByteLength())};
}

TEST(UDPReadableStreamWrapperTest, Create) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(
      MakeGarbageCollected<StreamCreator>(scope));
  auto* udp_readable_stream_wrapper = stream_creator->Create(scope);

  EXPECT_TRUE(udp_readable_stream_wrapper->Readable());
}

TEST(UDPReadableStreamWrapperTest, ReadUdpMessage) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(
      MakeGarbageCollected<StreamCreator>(scope));

  auto* udp_readable_stream_wrapper = stream_creator->Create(scope);
  auto& fake_udp_socket = stream_creator->fake_udp_socket();

  fake_udp_socket.ProvideRequestedDatagrams();

  auto* script_state = scope.GetScriptState();
  auto* reader =
      udp_readable_stream_wrapper->Readable()->GetDefaultReaderForTesting(
          script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester(script_state,
                             reader->read(script_state, ASSERT_NO_EXCEPTION));
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  auto [message, done] = UnpackPromiseResult(scope, tester.Value().V8Value());
  ASSERT_FALSE(done);
  ASSERT_TRUE(message->hasData());
  ASSERT_EQ(UDPMessageDataToString(message),
            fake_udp_socket.GetTestingDatagram());
}

TEST(UDPReadableStreamWrapperTest, ReadDelayedUdpMessage) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(
      MakeGarbageCollected<StreamCreator>(scope));
  auto* udp_readable_stream_wrapper = stream_creator->Create(scope);

  auto& fake_udp_socket = stream_creator->fake_udp_socket();

  auto* script_state = scope.GetScriptState();
  auto* reader =
      udp_readable_stream_wrapper->Readable()->GetDefaultReaderForTesting(
          script_state, ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(script_state,
                             reader->read(script_state, ASSERT_NO_EXCEPTION));

  fake_udp_socket.ProvideRequestedDatagrams();

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  auto [message, done] = UnpackPromiseResult(scope, tester.Value().V8Value());
  ASSERT_FALSE(done);
  ASSERT_TRUE(message->hasData());
  ASSERT_EQ(UDPMessageDataToString(message),
            fake_udp_socket.GetTestingDatagram());
}

TEST(UDPReadableStreamWrapperTest, ReadEmptyUdpMessage) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(
      MakeGarbageCollected<StreamCreator>(scope));
  auto* udp_readable_stream_wrapper = stream_creator->Create(scope);

  // Send empty datagrams.
  auto& fake_udp_socket = stream_creator->fake_udp_socket();
  fake_udp_socket.SetTestingDatagram({});
  fake_udp_socket.ProvideRequestedDatagrams();

  auto* script_state = scope.GetScriptState();
  auto* reader =
      udp_readable_stream_wrapper->Readable()->GetDefaultReaderForTesting(
          script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester tester(script_state,
                             reader->read(script_state, ASSERT_NO_EXCEPTION));
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  auto [message, done] = UnpackPromiseResult(scope, tester.Value().V8Value());
  ASSERT_FALSE(done);
  ASSERT_TRUE(message->hasData());

  ASSERT_EQ(UDPMessageDataToString(message).length(), 0U);
}

TEST(UDPReadableStreamWrapperTest, CancelStreamFromReader) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(
      MakeGarbageCollected<StreamCreator>(scope));
  auto* udp_readable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();
  auto* reader =
      udp_readable_stream_wrapper->Readable()->GetDefaultReaderForTesting(
          script_state, ASSERT_NO_EXCEPTION);

  ScriptPromiseTester cancel_tester(
      script_state, reader->cancel(script_state, ASSERT_NO_EXCEPTION));
  cancel_tester.WaitUntilSettled();
  EXPECT_TRUE(cancel_tester.IsFulfilled());

  ScriptPromiseTester read_tester(
      script_state, reader->read(script_state, ASSERT_NO_EXCEPTION));
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsFulfilled());

  auto [message, done] =
      UnpackPromiseResult(scope, read_tester.Value().V8Value());

  EXPECT_TRUE(done);
  EXPECT_FALSE(message);
}

TEST(UDPReadableStreamWrapperTest, ReadRejectsOnError) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ScopedStreamCreator stream_creator(
      MakeGarbageCollected<StreamCreator>(scope));
  auto* udp_readable_stream_wrapper = stream_creator->Create(scope);

  auto* script_state = scope.GetScriptState();
  auto* reader =
      udp_readable_stream_wrapper->Readable()->GetDefaultReaderForTesting(
          script_state, ASSERT_NO_EXCEPTION);

  udp_readable_stream_wrapper->ErrorStream(net::ERR_UNEXPECTED);

  ScriptPromiseTester read_tester(
      script_state, reader->read(script_state, ASSERT_NO_EXCEPTION));
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsRejected());
}

}  // namespace

}  // namespace blink
