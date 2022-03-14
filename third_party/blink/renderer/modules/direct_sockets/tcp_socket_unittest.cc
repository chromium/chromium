// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_socket.h"

#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

class TCPSocketCreator {
  STACK_ALLOCATED();

 public:
  TCPSocketCreator() = default;
  ~TCPSocketCreator() = default;

  TCPSocket* Create(const V8TestingScope& scope) {
    auto* script_state = scope.GetScriptState();
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    auto* tcp_socket =
        MakeGarbageCollected<TCPSocket>(scope.GetExecutionContext(), *resolver);
    create_promise_ = resolver->Promise();

    return tcp_socket;
  }

  ScriptPromise GetScriptPromise() { return create_promise_; }

 private:
  ScriptPromise create_promise_;
};

TEST(TCPSocketTest, Create) {
  V8TestingScope scope;
  TCPSocketCreator tcp_socket_creator;

  auto create_promise = tcp_socket_creator.GetScriptPromise();
  EXPECT_TRUE(create_promise.IsEmpty());

  tcp_socket_creator.Create(scope);

  auto* script_state = scope.GetScriptState();
  create_promise = tcp_socket_creator.GetScriptPromise();
  ScriptPromiseTester create_tester(script_state, create_promise);
  EXPECT_TRUE(create_promise.IsAssociatedWith(script_state));

  ASSERT_FALSE(create_tester.IsFulfilled());
}

TEST(TCPSocketTest, CloseBeforeInit) {
  V8TestingScope scope;
  TCPSocketCreator tcp_socket_creator;

  auto* tcp_socket = tcp_socket_creator.Create(scope);
  auto* script_state = scope.GetScriptState();
  auto create_promise = tcp_socket_creator.GetScriptPromise();
  ScriptPromiseTester create_tester(script_state, create_promise);
  ASSERT_FALSE(create_tester.IsRejected());

  auto close_promise =
      tcp_socket->close(script_state, scope.GetExceptionState());
  ScriptPromiseTester close_tester(script_state, close_promise);

  create_tester.WaitUntilSettled();
  ASSERT_TRUE(create_tester.IsRejected());

  DOMException* create_exception = V8DOMException::ToImplWithTypeCheck(
      scope.GetIsolate(), create_tester.Value().V8Value());
  ASSERT_TRUE(create_exception);
  EXPECT_EQ(create_exception->name(), "AbortError");
  EXPECT_EQ(create_exception->message(), "The request was aborted locally");

  close_tester.WaitUntilSettled();
  ASSERT_TRUE(close_tester.IsFulfilled());
}

TEST(TCPSocketTest, CloseAfterInitWithoutResultOK) {
  V8TestingScope scope;
  TCPSocketCreator tcp_socket_creator;

  auto* tcp_socket = tcp_socket_creator.Create(scope);
  auto* script_state = scope.GetScriptState();
  auto create_promise = tcp_socket_creator.GetScriptPromise();
  ScriptPromiseTester create_tester(script_state, create_promise);
  ASSERT_FALSE(create_tester.IsRejected());

  int32_t result = net::Error::ERR_FAILED;
  tcp_socket->Init(result, net::IPEndPoint(), net::IPEndPoint(),
                   mojo::ScopedDataPipeConsumerHandle(),
                   mojo::ScopedDataPipeProducerHandle());

  auto close_promise =
      tcp_socket->close(script_state, scope.GetExceptionState());
  ScriptPromiseTester close_tester(script_state, close_promise);

  create_tester.WaitUntilSettled();
  ASSERT_TRUE(create_tester.IsRejected());

  DOMException* create_exception = V8DOMException::ToImplWithTypeCheck(
      scope.GetIsolate(), create_tester.Value().V8Value());
  ASSERT_TRUE(create_exception);
  EXPECT_EQ(create_exception->name(), "NetworkError");
  EXPECT_EQ(create_exception->message(), "Network Error.");

  close_tester.WaitUntilSettled();
  ASSERT_TRUE(close_tester.IsFulfilled());
}

TEST(TCPSocketTest, CloseAfterInitWithResultOK) {
  V8TestingScope scope;
  TCPSocketCreator tcp_socket_creator;

  auto* tcp_socket = tcp_socket_creator.Create(scope);
  auto* script_state = scope.GetScriptState();
  auto create_promise = tcp_socket_creator.GetScriptPromise();
  ScriptPromiseTester create_tester(script_state, create_promise);
  ASSERT_FALSE(create_tester.IsFulfilled());

  int32_t result = net::Error::OK;
  tcp_socket->Init(result, net::IPEndPoint(), net::IPEndPoint(),
                   mojo::ScopedDataPipeConsumerHandle(),
                   mojo::ScopedDataPipeProducerHandle());
  EXPECT_TRUE(tcp_socket->readable());
  EXPECT_TRUE(tcp_socket->writable());

  auto close_promise =
      tcp_socket->close(script_state, scope.GetExceptionState());
  ScriptPromiseTester close_tester(script_state, close_promise);

  create_tester.WaitUntilSettled();
  ASSERT_TRUE(create_tester.IsFulfilled());
  close_tester.WaitUntilSettled();
  ASSERT_TRUE(close_tester.IsFulfilled());
}

TEST(TCPSocketTest, OnSocketObserverConnectionError) {
  V8TestingScope scope;
  TCPSocketCreator tcp_socket_creator;

  auto* tcp_socket = tcp_socket_creator.Create(scope);
  auto* script_state = scope.GetScriptState();
  auto create_promise = tcp_socket_creator.GetScriptPromise();
  ScriptPromiseTester create_tester(script_state, create_promise);
  ASSERT_FALSE(create_tester.IsRejected());

  // Trigger OnSocketObserverConnectionError().
  auto observer = tcp_socket->GetTCPSocketObserver();
  observer.reset();

  create_tester.WaitUntilSettled();
  ASSERT_TRUE(create_tester.IsRejected());

  DOMException* create_exception = V8DOMException::ToImplWithTypeCheck(
      scope.GetIsolate(), create_tester.Value().V8Value());
  ASSERT_TRUE(create_exception);
  EXPECT_EQ(create_exception->name(), "NetworkError");
  EXPECT_EQ(create_exception->message(),
            "The request was aborted due to connection error");
}

}  // namespace

}  // namespace blink
