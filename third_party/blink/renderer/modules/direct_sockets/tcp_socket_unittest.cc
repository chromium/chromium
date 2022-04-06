// Copyright 2020 The Chromium Authors. All rights reserved.
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
#include "third_party/blink/renderer/bindings/modules/v8/v8_socket_close_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/direct_sockets/tcp_readable_stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
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
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* tcp_socket = MakeGarbageCollected<TCPSocket>(script_state);

  auto close_promise =
      tcp_socket->close(script_state, blink::SocketCloseOptions::Create(),
                        scope.GetExceptionState());

  ASSERT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST(TCPSocketTest, CloseAfterInitWithoutResultOK) {
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* tcp_socket = MakeGarbageCollected<TCPSocket>(script_state);

  auto connection_promise = tcp_socket->connection(script_state);
  ScriptPromiseTester connection_tester(script_state, connection_promise);

  tcp_socket->Init(net::ERR_FAILED, net::IPEndPoint(), net::IPEndPoint(),
                   mojo::ScopedDataPipeConsumerHandle(),
                   mojo::ScopedDataPipeProducerHandle());

  connection_tester.WaitUntilSettled();
  ASSERT_TRUE(connection_tester.IsRejected());

  auto close_promise =
      tcp_socket->close(script_state, blink::SocketCloseOptions::Create(),
                        scope.GetExceptionState());

  ASSERT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST(TCPSocketTest, CloseAfterInitWithResultOK) {
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* tcp_socket = MakeGarbageCollected<TCPSocket>(script_state);

  auto connection_promise = tcp_socket->connection(script_state);
  ScriptPromiseTester connection_tester(script_state, connection_promise);

  auto [_, consumer] = CreateDataPipe();
  tcp_socket->Init(net::OK, net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0},
                   net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0},
                   std::move(consumer), mojo::ScopedDataPipeProducerHandle());

  connection_tester.WaitUntilSettled();
  ASSERT_TRUE(connection_tester.IsFulfilled());

  auto close_promise =
      tcp_socket->close(script_state, blink::SocketCloseOptions::Create(),
                        scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());
}

TEST(TCPSocketTest, OnSocketObserverConnectionError) {
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* tcp_socket = MakeGarbageCollected<TCPSocket>(script_state);

  auto connection_promise = tcp_socket->connection(script_state);
  ScriptPromiseTester connection_tester(script_state, connection_promise);

  auto [_, consumer] = CreateDataPipe();
  tcp_socket->Init(net::OK, net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0},
                   net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0},
                   std::move(consumer), mojo::ScopedDataPipeProducerHandle());

  connection_tester.WaitUntilSettled();
  ASSERT_TRUE(connection_tester.IsFulfilled());

  ScriptPromiseTester closed_tester(script_state,
                                    tcp_socket->closed(script_state));

  // Trigger OnSocketObserverConnectionError().
  auto observer = tcp_socket->GetTCPSocketObserver();
  observer.reset();

  closed_tester.WaitUntilSettled();
  ASSERT_TRUE(closed_tester.IsRejected());
}

TEST(TCPSocketTest, OnReadError) {
  V8TestingScope scope;

  auto* script_state = scope.GetScriptState();
  auto* tcp_socket = MakeGarbageCollected<TCPSocket>(script_state);

  auto connection_promise = tcp_socket->connection(script_state);
  ScriptPromiseTester connection_tester(script_state, connection_promise);

  auto [_, consumer] = CreateDataPipe();
  tcp_socket->Init(net::OK, net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0},
                   net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0},
                   std::move(consumer), mojo::ScopedDataPipeProducerHandle());

  connection_tester.WaitUntilSettled();
  ASSERT_TRUE(connection_tester.IsFulfilled());

  ScriptPromiseTester closed_tester(script_state,
                                    tcp_socket->closed(script_state));

  tcp_socket->OnReadError(net::ERR_UNEXPECTED);

  ASSERT_EQ(tcp_socket->readable_stream_wrapper_->GetState(),
            StreamWrapper::State::kAborted);
}

}  // namespace blink
