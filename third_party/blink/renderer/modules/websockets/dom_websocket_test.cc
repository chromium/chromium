// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/dom_websocket.h"

#include <memory>
#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/websockets/mock_websocket_channel.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::Ref;
using testing::Return;

namespace blink {

namespace {

typedef testing::StrictMock<testing::MockFunction<void(int)>>
    Checkpoint;  // NOLINT

class DOMWebSocketWithMockChannel final : public DOMWebSocket {
 public:
  static DOMWebSocketWithMockChannel* Create(ExecutionContext* context) {
    DOMWebSocketWithMockChannel* websocket =
        MakeGarbageCollected<DOMWebSocketWithMockChannel>(context);
    websocket->UpdateStateIfNeeded();
    return websocket;
  }

  explicit DOMWebSocketWithMockChannel(ExecutionContext* context)
      : DOMWebSocket(context),
        channel_(MakeGarbageCollected<MockWebSocketChannel>()),
        has_created_channel_(false) {}

  MockWebSocketChannel* Channel() { return channel_.Get(); }

  WebSocketChannel* CreateChannel(ExecutionContext*,
                                  WebSocketChannelClient*) override {
    DCHECK(!has_created_channel_);
    has_created_channel_ = true;
    return channel_.Get();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(channel_);
    DOMWebSocket::Trace(visitor);
  }

 private:
  Member<MockWebSocketChannel> channel_;
  bool has_created_channel_;
};

class DOMWebSocketTestScope {
  STACK_ALLOCATED();

 public:
  explicit DOMWebSocketTestScope(ExecutionContext* execution_context)
      : websocket_(DOMWebSocketWithMockChannel::Create(execution_context)) {}

  ~DOMWebSocketTestScope() {
    if (!websocket_)
      return;
    // These statements are needed to clear WebSocket::channel_ to
    // avoid ASSERTION failure on ~DOMWebSocket.
    DCHECK(Socket().Channel());
    testing::Mock::VerifyAndClear(Socket().Channel());
    EXPECT_CALL(Channel(), Disconnect()).Times(AnyNumber());

    Socket().DidClose(WebSocketChannelClient::kClosingHandshakeIncomplete, 1006,
                      "");
  }

  MockWebSocketChannel& Channel() { return *websocket_->Channel(); }
  DOMWebSocketWithMockChannel& Socket() { return *websocket_.Get(); }

 private:
  Persistent<DOMWebSocketWithMockChannel> websocket_;
};

TEST(DOMWebSocketTest, connectToBadURL) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  websocket_scope.Socket().Connect("xxx", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMExceptionCode::kSyntaxError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
  EXPECT_EQ("The URL 'xxx' is invalid.", scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kClosed, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, connectToNonWsURL) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  websocket_scope.Socket().Connect("bad-scheme://example.com/",
                                   Vector<String>(), scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMExceptionCode::kSyntaxError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
  EXPECT_EQ(
      "The URL's scheme must be either 'http', 'https', 'ws', or 'wss'. "
      "'bad-scheme' is not allowed.",
      scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kClosed, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, connectToURLHavingFragmentIdentifier) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  websocket_scope.Socket().Connect("ws://example.com/#fragment",
                                   Vector<String>(), scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMExceptionCode::kSyntaxError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
  EXPECT_EQ(
      "The URL contains a fragment identifier ('fragment'). Fragment "
      "identifiers are not allowed in WebSocket URLs.",
      scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kClosed, websocket_scope.Socket().readyState());
}

// FIXME: Add a test for Content Security Policy.

TEST(DOMWebSocketTest, invalidSubprotocols) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  Vector<String> subprotocols;
  subprotocols.push_back("@subprotocol-|'\"x\x01\x02\x03x");

  websocket_scope.Socket().Connect("ws://example.com/", subprotocols,
                                   scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMExceptionCode::kSyntaxError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
  EXPECT_EQ(
      "The subprotocol '@subprotocol-|'\"x\\u0001\\u0002\\u0003x' is invalid.",
      scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kClosed, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, insecureRequestsUpgrade) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("wss://example.com/endpoint"), String()))
        .WillOnce(Return(true));
  }

  scope.GetWindow().GetSecurityContext().SetInsecureRequestPolicy(
      mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests);
  websocket_scope.Socket().Connect("ws://example.com/endpoint",
                                   Vector<String>(), scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());
  EXPECT_EQ(KURL("wss://example.com/endpoint"), websocket_scope.Socket().url());
}

TEST(DOMWebSocketTest, insecureRequestsUpgradePotentiallyTrustworthy) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://127.0.0.1/endpoint"), String()))
        .WillOnce(Return(true));
  }

  scope.GetWindow().GetSecurityContext().SetInsecureRequestPolicy(
      mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests);
  websocket_scope.Socket().Connect("ws://127.0.0.1/endpoint", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());
  EXPECT_EQ(KURL("ws://127.0.0.1/endpoint"), websocket_scope.Socket().url());
}

TEST(DOMWebSocketTest, insecureRequestsDoNotUpgrade) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/endpoint"), String()))
        .WillOnce(Return(true));
  }

  scope.GetWindow().GetSecurityContext().SetInsecureRequestPolicy(
      mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone);
  websocket_scope.Socket().Connect("ws://example.com/endpoint",
                                   Vector<String>(), scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());
  EXPECT_EQ(KURL("ws://example.com/endpoint"), websocket_scope.Socket().url());
}

TEST(DOMWebSocketTest, channelConnectSuccess) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  Vector<String> subprotocols;
  subprotocols.push_back("aa");
  subprotocols.push_back("bb");

  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/hoge"), String("aa, bb")))
        .WillOnce(Return(true));
  }

  websocket_scope.Socket().Connect("ws://example.com/hoge",
                                   Vector<String>(subprotocols),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());
  EXPECT_EQ(KURL("ws://example.com/hoge"), websocket_scope.Socket().url());
}

TEST(DOMWebSocketTest, channelConnectFail) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  Vector<String> subprotocols;
  subprotocols.push_back("aa");
  subprotocols.push_back("bb");

  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String("aa, bb")))
        .WillOnce(Return(false));
    EXPECT_CALL(websocket_scope.Channel(), Disconnect());
  }

  websocket_scope.Socket().Connect("ws://example.com/",
                                   Vector<String>(subprotocols),
                                   scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMExceptionCode::kSecurityError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
  EXPECT_EQ(
      "An insecure WebSocket connection may not be initiated from a page "
      "loaded over HTTPS.",
      scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kClosed, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, connectSuccess) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  Vector<String> subprotocols;
  subprotocols.push_back("aa");
  subprotocols.push_back("bb");
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String("aa, bb")))
        .WillOnce(Return(true));
  }
  websocket_scope.Socket().Connect("ws://example.com/", subprotocols,
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());

  websocket_scope.Socket().DidConnect("bb", "cc");

  EXPECT_EQ(DOMWebSocket::kOpen, websocket_scope.Socket().readyState());
  EXPECT_EQ("bb", websocket_scope.Socket().protocol());
  EXPECT_EQ("cc", websocket_scope.Socket().extensions());
}

TEST(DOMWebSocketTest, didClose) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), Disconnect());
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());

  websocket_scope.Socket().DidClose(
      WebSocketChannelClient::kClosingHandshakeIncomplete, 1006, "");

  EXPECT_EQ(DOMWebSocket::kClosed, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, maximumReasonSize) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), FailMock(_, _, _));
  }
  StringBuilder reason;
  for (size_t i = 0; i < 123; ++i)
    reason.Append('a');
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());

  websocket_scope.Socket().close(1000, reason.ToString(),
                                 scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, reasonSizeExceeding) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
  }
  StringBuilder reason;
  for (size_t i = 0; i < 124; ++i)
    reason.Append('a');
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());

  websocket_scope.Socket().close(1000, reason.ToString(),
                                 scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMExceptionCode::kSyntaxError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
  EXPECT_EQ("The close reason must not be greater than 123 UTF-8 bytes.",
            scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, closeWhenConnecting) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(
        websocket_scope.Channel(),
        FailMock(
            String("WebSocket is closed before the connection is established."),
            mojom::ConsoleMessageLevel::kWarning, _));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());

  websocket_scope.Socket().close(1000, "bye", scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, close) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), Close(3005, String("bye")));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());

  websocket_scope.Socket().DidConnect("", "");
  EXPECT_EQ(DOMWebSocket::kOpen, websocket_scope.Socket().readyState());
  websocket_scope.Socket().close(3005, "bye", scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, closeWithoutReason) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), Close(3005, String()));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());

  websocket_scope.Socket().DidConnect("", "");
  EXPECT_EQ(DOMWebSocket::kOpen, websocket_scope.Socket().readyState());
  websocket_scope.Socket().close(3005, scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, closeWithoutCodeAndReason) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), Close(-1, String()));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());

  websocket_scope.Socket().DidConnect("", "");
  EXPECT_EQ(DOMWebSocket::kOpen, websocket_scope.Socket().readyState());
  websocket_scope.Socket().close(scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, closeWhenClosing) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), Close(-1, String()));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());

  websocket_scope.Socket().DidConnect("", "");
  EXPECT_EQ(DOMWebSocket::kOpen, websocket_scope.Socket().readyState());
  websocket_scope.Socket().close(scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, websocket_scope.Socket().readyState());

  websocket_scope.Socket().close(scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, closeWhenClosed) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), Close(-1, String()));
    EXPECT_CALL(websocket_scope.Channel(), Disconnect());
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());

  websocket_scope.Socket().DidConnect("", "");
  EXPECT_EQ(DOMWebSocket::kOpen, websocket_scope.Socket().readyState());
  websocket_scope.Socket().close(scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, websocket_scope.Socket().readyState());

  websocket_scope.Socket().DidClose(
      WebSocketChannelClient::kClosingHandshakeComplete, 1000, String());
  EXPECT_EQ(DOMWebSocket::kClosed, websocket_scope.Socket().readyState());
  websocket_scope.Socket().close(scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosed, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendStringWhenConnecting) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  websocket_scope.Socket().send("hello", scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
  EXPECT_EQ("Still in CONNECTING state.", scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendStringWhenClosing) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), FailMock(_, _, _));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  websocket_scope.Socket().close(scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  websocket_scope.Socket().send("hello", scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendStringWhenClosed) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  Checkpoint checkpoint;
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), Disconnect());
    EXPECT_CALL(checkpoint, Call(1));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  websocket_scope.Socket().DidClose(
      WebSocketChannelClient::kClosingHandshakeIncomplete, 1006, "");
  checkpoint.Call(1);

  websocket_scope.Socket().send("hello", scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosed, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendStringSuccess) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), Send(std::string("hello"), _));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  websocket_scope.Socket().DidConnect("", "");
  websocket_scope.Socket().send("hello", scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kOpen, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendNonLatin1String) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(),
                Send(std::string("\xe7\x8b\x90\xe0\xa4\x94"), _));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  websocket_scope.Socket().DidConnect("", "");
  UChar non_latin1_string[] = {0x72d0, 0x0914, 0x0000};
  websocket_scope.Socket().send(non_latin1_string, scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kOpen, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendArrayBufferWhenConnecting) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  DOMArrayBufferView* view = DOMUint8Array::Create(8);
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  websocket_scope.Socket().send(view->buffer(), scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
  EXPECT_EQ("Still in CONNECTING state.", scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendArrayBufferWhenClosing) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  DOMArrayBufferView* view = DOMUint8Array::Create(8);
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), FailMock(_, _, _));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  websocket_scope.Socket().close(scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  websocket_scope.Socket().send(view->buffer(), scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendArrayBufferWhenClosed) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  Checkpoint checkpoint;
  DOMArrayBufferView* view = DOMUint8Array::Create(8);
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), Disconnect());
    EXPECT_CALL(checkpoint, Call(1));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  websocket_scope.Socket().DidClose(
      WebSocketChannelClient::kClosingHandshakeIncomplete, 1006, "");
  checkpoint.Call(1);

  websocket_scope.Socket().send(view->buffer(), scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosed, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, sendArrayBufferSuccess) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  DOMArrayBufferView* view = DOMUint8Array::Create(8);
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), Send(Ref(*view->buffer()), 0, 8, _));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  websocket_scope.Socket().DidConnect("", "");
  websocket_scope.Socket().send(view->buffer(), scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kOpen, websocket_scope.Socket().readyState());
}

// FIXME: We should have Blob tests here.
// We can't create a Blob because the blob registration cannot be mocked yet.

TEST(DOMWebSocketTest, bufferedAmountUpdated) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), Send(std::string("hello"), _));
    EXPECT_CALL(websocket_scope.Channel(), Send(std::string("world"), _));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  websocket_scope.Socket().DidConnect("", "");
  websocket_scope.Socket().send("hello", scope.GetExceptionState());
  EXPECT_EQ(websocket_scope.Socket().bufferedAmount(), 5u);
  websocket_scope.Socket().send("world", scope.GetExceptionState());
  EXPECT_EQ(websocket_scope.Socket().bufferedAmount(), 10u);
  websocket_scope.Socket().DidConsumeBufferedAmount(5);
  websocket_scope.Socket().DidConsumeBufferedAmount(5);
  EXPECT_EQ(websocket_scope.Socket().bufferedAmount(), 10u);
  blink::test::RunPendingTasks();
  EXPECT_EQ(websocket_scope.Socket().bufferedAmount(), 0u);

  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST(DOMWebSocketTest, bufferedAmountUpdatedBeforeOnMessage) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), Send(std::string("hello"), _));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());

  websocket_scope.Socket().DidConnect("", "");
  // send() is called from onopen
  websocket_scope.Socket().send("hello", scope.GetExceptionState());
  // (return to event loop)
  websocket_scope.Socket().DidConsumeBufferedAmount(5);
  EXPECT_EQ(websocket_scope.Socket().bufferedAmount(), 5ul);
  // New message was already queued, is processed before task posted from
  // DidConsumeBufferedAmount().
  websocket_scope.Socket().DidReceiveTextMessage("hello");
  // bufferedAmount is observed inside onmessage event handler.
  EXPECT_EQ(websocket_scope.Socket().bufferedAmount(), 0ul);

  blink::test::RunPendingTasks();
  EXPECT_EQ(websocket_scope.Socket().bufferedAmount(), 0ul);

  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

// FIXME: We should add tests for data receiving.

TEST(DOMWebSocketTest, binaryType) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  EXPECT_EQ("blob", websocket_scope.Socket().binaryType());

  websocket_scope.Socket().setBinaryType(
      V8BinaryType(V8BinaryType::Enum::kArraybuffer));

  EXPECT_EQ("arraybuffer", websocket_scope.Socket().binaryType().AsString());

  websocket_scope.Socket().setBinaryType(
      V8BinaryType(V8BinaryType::Enum::kBlob));

  EXPECT_EQ("blob", websocket_scope.Socket().binaryType().AsString());
}

// FIXME: We should add tests for suspend / resume.

class DOMWebSocketValidClosingTest : public testing::TestWithParam<uint16_t> {
  test::TaskEnvironment task_environment_;
};

TEST_P(DOMWebSocketValidClosingTest, test) {
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), FailMock(_, _, _));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());

  websocket_scope.Socket().close(GetParam(), "bye", scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kClosing, websocket_scope.Socket().readyState());
}

INSTANTIATE_TEST_SUITE_P(DOMWebSocketValidClosing,
                         DOMWebSocketValidClosingTest,
                         testing::Values(1000, 3000, 3001, 4998, 4999));

class DOMWebSocketInvalidClosingCodeTest
    : public testing::TestWithParam<uint16_t> {
  test::TaskEnvironment task_environment_;
};

TEST_P(DOMWebSocketInvalidClosingCodeTest, test) {
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
  }
  websocket_scope.Socket().Connect("ws://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());

  websocket_scope.Socket().close(GetParam(), "bye", scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMExceptionCode::kInvalidAccessError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
  EXPECT_EQ(String::Format("The close code must be either 1000, or between "
                           "3000 and 4999. %d is neither.",
                           GetParam()),
            scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());
}

INSTANTIATE_TEST_SUITE_P(
    DOMWebSocketInvalidClosingCode,
    DOMWebSocketInvalidClosingCodeTest,
    testing::Values(0, 1, 998, 999, 1001, 2999, 5000, 9999, 65535));

TEST(DOMWebSocketTest, GCWhileEventsPending) {
  test::TaskEnvironment task_environment_;
  V8TestingScope scope;
  {
    DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());

    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), Disconnect());

    auto& socket = websocket_scope.Socket();

    // Cause events to be queued rather than fired.
    socket.ContextLifecycleStateChanged(mojom::FrameLifecycleState::kPaused);

    socket.Connect("ws://example.com/", Vector<String>(), ASSERT_NO_EXCEPTION);
    socket.DidError();
    socket.DidClose(DOMWebSocket::kClosingHandshakeIncomplete, 1006, "");

    // Stop HasPendingActivity() from keeping the object alive.
    socket.SetExecutionContext(nullptr);
  }

  ThreadState::Current()->CollectAllGarbageForTesting();
}

}  // namespace

}  // namespace blink
