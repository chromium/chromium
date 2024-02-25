// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_socket.h"

#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/modules/direct_sockets/tcp_readable_stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

std::pair<mojo::ScopedDataPipeProducerHandle,
          mojo::ScopedDataPipeConsumerHandle>
CreateDataPipe(int32_t capacity = 1) {
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  mojo::CreateDataPipe(capacity, producer, consumer);

  return {std::move(producer), std::move(consumer)};
}

}  // namespace

TEST(TCPSocketTest, CloseBeforeInit) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* tcp_socket = MakeGarbageCollected<TCPSocket>(script_state);

  auto close_promise =
      tcp_socket->close(script_state, scope.GetExceptionState());

  ASSERT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST(TCPSocketTest, CloseAfterInitWithResultOK) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* tcp_socket = MakeGarbageCollected<TCPSocket>(script_state);

  auto opened_promise = tcp_socket->opened(script_state);
  ScriptPromiseTester opened_tester(script_state, opened_promise);

  auto [consumer_complement, consumer] = CreateDataPipe();
  auto [producer, producer_complement] = CreateDataPipe();

  mojo::PendingReceiver<network::mojom::blink::TCPConnectedSocket>
      socket_receiver;
  mojo::PendingRemote<network::mojom::blink::SocketObserver> observer_remote;

  tcp_socket->OnTCPSocketOpened(
      socket_receiver.InitWithNewPipeAndPassRemote(),
      observer_remote.InitWithNewPipeAndPassReceiver(), net::OK,
      net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0},
      net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0}, std::move(consumer),
      std::move(producer));

  opened_tester.WaitUntilSettled();
  ASSERT_TRUE(opened_tester.IsFulfilled());

  auto close_promise =
      tcp_socket->close(script_state, scope.GetExceptionState());
  test::RunPendingTasks();
  ASSERT_FALSE(scope.GetExceptionState().HadException());
}

TEST(TCPSocketTest, OnSocketObserverConnectionError) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* tcp_socket = MakeGarbageCollected<TCPSocket>(script_state);

  auto opened_promise = tcp_socket->opened(script_state);
  ScriptPromiseTester opened_tester(script_state, opened_promise);

  auto [consumer_complement, consumer] = CreateDataPipe();
  auto [producer, producer_complement] = CreateDataPipe();

  mojo::PendingReceiver<network::mojom::blink::TCPConnectedSocket>
      socket_receiver;
  mojo::PendingRemote<network::mojom::blink::SocketObserver> observer_remote;

  tcp_socket->OnTCPSocketOpened(
      socket_receiver.InitWithNewPipeAndPassRemote(),
      observer_remote.InitWithNewPipeAndPassReceiver(), net::OK,
      net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0},
      net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0}, std::move(consumer),
      std::move(producer));

  opened_tester.WaitUntilSettled();
  ASSERT_TRUE(opened_tester.IsFulfilled());

  ScriptPromiseTester closed_tester(script_state,
                                    tcp_socket->closed(script_state));

  // Trigger OnSocketObserverConnectionError().
  observer_remote.reset();
  consumer_complement.reset();
  producer_complement.reset();

  closed_tester.WaitUntilSettled();
  ASSERT_TRUE(closed_tester.IsRejected());
}

class TCPSocketCloseTest
    : public testing::TestWithParam<std::tuple<bool, bool>> {};

TEST_P(TCPSocketCloseTest, OnErrorOrClose) {
  auto [read_error, write_error] = GetParam();

  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* tcp_socket = MakeGarbageCollected<TCPSocket>(script_state);

  auto opened_promise = tcp_socket->opened(script_state);
  ScriptPromiseTester opened_tester(script_state, opened_promise);

  auto [consumer_complement, consumer] = CreateDataPipe();
  auto [producer, producer_complement] = CreateDataPipe();

  mojo::PendingReceiver<network::mojom::blink::TCPConnectedSocket>
      socket_receiver;
  mojo::PendingRemote<network::mojom::blink::SocketObserver> observer_remote;

  tcp_socket->OnTCPSocketOpened(
      socket_receiver.InitWithNewPipeAndPassRemote(),
      observer_remote.InitWithNewPipeAndPassReceiver(), net::OK,
      net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0},
      net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0}, std::move(consumer),
      std::move(producer));

  opened_tester.WaitUntilSettled();
  ASSERT_TRUE(opened_tester.IsFulfilled());

  ScriptPromiseTester closed_tester(script_state,
                                    tcp_socket->closed(script_state));

  if (read_error) {
    tcp_socket->OnReadError(net::ERR_UNEXPECTED);
    consumer_complement.reset();
    test::RunPendingTasks();
  } else {
    auto* readable = tcp_socket->readable_stream_wrapper_->Readable();
    auto cancel = ScriptPromiseTester(
        script_state, readable->cancel(script_state, ASSERT_NO_EXCEPTION));
    cancel.WaitUntilSettled();
    ASSERT_TRUE(cancel.IsFulfilled());
  }

  ASSERT_EQ(tcp_socket->readable_stream_wrapper_->GetState(),
            read_error ? StreamWrapper::State::kAborted
                       : StreamWrapper::State::kClosed);

  if (write_error) {
    tcp_socket->OnWriteError(net::ERR_UNEXPECTED);
    producer_complement.reset();
    test::RunPendingTasks();
  } else {
    auto* writable = tcp_socket->writable_stream_wrapper_->Writable();
    auto abort = ScriptPromiseTester(
        script_state, writable->abort(script_state, ASSERT_NO_EXCEPTION));
    abort.WaitUntilSettled();
    ASSERT_TRUE(abort.IsFulfilled());
  }

  ASSERT_EQ(tcp_socket->writable_stream_wrapper_->GetState(),
            write_error ? StreamWrapper::State::kAborted
                        : StreamWrapper::State::kClosed);

  closed_tester.WaitUntilSettled();
  if (!read_error && !write_error) {
    ASSERT_TRUE(closed_tester.IsFulfilled());
  } else {
    ASSERT_TRUE(closed_tester.IsRejected());
  }
}

INSTANTIATE_TEST_SUITE_P(/**/,
                         TCPSocketCloseTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace blink
