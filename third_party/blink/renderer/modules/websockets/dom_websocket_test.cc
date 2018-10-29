// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/dom_websocket.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/inspector/console_types.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/cstring.h"
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

class MockWebSocketChannel : public WebSocketChannel {
 public:
  static MockWebSocketChannel* Create() {
    return new testing::StrictMock<MockWebSocketChannel>();
  }

  ~MockWebSocketChannel() override = default;

  MOCK_METHOD2(Connect, bool(const KURL&, const String&));
  MOCK_METHOD1(Send, void(const CString&));
  MOCK_METHOD3(Send, void(const DOMArrayBuffer&, unsigned, unsigned));
  MOCK_METHOD1(SendMock, void(BlobDataHandle*));
  void Send(scoped_refptr<BlobDataHandle> handle) override {
    SendMock(handle.get());
  }
  MOCK_METHOD1(SendTextAsCharVectorMock, void(Vector<char>*));
  void SendTextAsCharVector(std::unique_ptr<Vector<char>> vector) override {
    SendTextAsCharVectorMock(vector.get());
  }
  MOCK_METHOD1(SendBinaryAsCharVectorMock, void(Vector<char>*));
  void SendBinaryAsCharVector(std::unique_ptr<Vector<char>> vector) override {
    SendBinaryAsCharVectorMock(vector.get());
  }
  MOCK_CONST_METHOD0(BufferedAmount, unsigned());
  MOCK_METHOD2(Close, void(int, const String&));
  MOCK_METHOD3(FailMock, void(const String&, MessageLevel, SourceLocation*));
  void Fail(const String& reason,
            MessageLevel level,
            std::unique_ptr<SourceLocation> location) override {
    FailMock(reason, level, location.get());
  }
  MOCK_METHOD0(Disconnect, void());

  MockWebSocketChannel() = default;
};

class DOMWebSocketWithMockChannel final : public DOMWebSocket {
 public:
  static DOMWebSocketWithMockChannel* Create(ExecutionContext* context) {
    DOMWebSocketWithMockChannel* websocket =
        new DOMWebSocketWithMockChannel(context);
    websocket->PauseIfNeeded();
    return websocket;
  }

  MockWebSocketChannel* Channel() { return channel_.Get(); }

  WebSocketChannel* CreateChannel(ExecutionContext*,
                                  WebSocketChannelClient*) override {
    DCHECK(!has_created_channel_);
    has_created_channel_ = true;
    return channel_.Get();
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(channel_);
    DOMWebSocket::Trace(visitor);
  }

 private:
  explicit DOMWebSocketWithMockChannel(ExecutionContext* context)
      : DOMWebSocket(context),
        channel_(MockWebSocketChannel::Create()),
        has_created_channel_(false) {}

  Member<MockWebSocketChannel> channel_;
  bool has_created_channel_;
};

class DOMWebSocketTestScope {
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
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  websocket_scope.Socket().Connect("http://example.com/", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMExceptionCode::kSyntaxError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
  EXPECT_EQ(
      "The URL's scheme must be either 'ws' or 'wss'. 'http' is not allowed.",
      scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kClosed, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, connectToURLHavingFragmentIdentifier) {
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

TEST(DOMWebSocketTest, invalidPort) {
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  websocket_scope.Socket().Connect("ws://example.com:7", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMExceptionCode::kSecurityError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
  EXPECT_EQ("The port 7 is not allowed.", scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kClosed, websocket_scope.Socket().readyState());
}

// FIXME: Add a test for Content Security Policy.

TEST(DOMWebSocketTest, invalidSubprotocols) {
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
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("wss://example.com/endpoint"), String()))
        .WillOnce(Return(true));
  }

  scope.GetDocument().SetInsecureRequestPolicy(kUpgradeInsecureRequests);
  websocket_scope.Socket().Connect("ws://example.com/endpoint",
                                   Vector<String>(), scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());
  EXPECT_EQ(KURL("wss://example.com/endpoint"), websocket_scope.Socket().url());
}

TEST(DOMWebSocketTest, insecureRequestsUpgradePotentiallyTrustworthy) {
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://127.0.0.1/endpoint"), String()))
        .WillOnce(Return(true));
  }

  scope.GetDocument().SetInsecureRequestPolicy(kUpgradeInsecureRequests);
  websocket_scope.Socket().Connect("ws://127.0.0.1/endpoint", Vector<String>(),
                                   scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());
  EXPECT_EQ(KURL("ws://127.0.0.1/endpoint"), websocket_scope.Socket().url());
}

TEST(DOMWebSocketTest, insecureRequestsDoNotUpgrade) {
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/endpoint"), String()))
        .WillOnce(Return(true));
  }

  scope.GetDocument().SetInsecureRequestPolicy(kLeaveInsecureRequestsAlone);
  websocket_scope.Socket().Connect("ws://example.com/endpoint",
                                   Vector<String>(), scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());
  EXPECT_EQ(KURL("ws://example.com/endpoint"), websocket_scope.Socket().url());
}

TEST(DOMWebSocketTest, mixedContentAutoUpgrade) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kMixedContentAutoupgrade);
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("wss://example.com/endpoint"), String()))
        .WillOnce(Return(true));
  }
  scope.GetDocument().SetURL(KURL("https://example.com"));
  scope.GetDocument().SetInsecureRequestPolicy(kLeaveInsecureRequestsAlone);
  websocket_scope.Socket().Connect("ws://example.com/endpoint",
                                   Vector<String>(), scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());
  EXPECT_EQ(KURL("wss://example.com/endpoint"), websocket_scope.Socket().url());
}

TEST(DOMWebSocketTest, channelConnectSuccess) {
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

TEST(DOMWebSocketTest, isValidSubprotocolString) {
  EXPECT_TRUE(DOMWebSocket::IsValidSubprotocolString("Helloworld!!"));
  EXPECT_FALSE(DOMWebSocket::IsValidSubprotocolString("Hello, world!!"));
  EXPECT_FALSE(DOMWebSocket::IsValidSubprotocolString(String()));
  EXPECT_FALSE(DOMWebSocket::IsValidSubprotocolString(""));

  const char kValidCharacters[] =
      "!#$%&'*+-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`"
      "abcdefghijklmnopqrstuvwxyz|~";
  size_t length = strlen(kValidCharacters);
  for (size_t i = 0; i < length; ++i) {
    String s;
    s.append(static_cast<UChar>(kValidCharacters[i]));
    EXPECT_TRUE(DOMWebSocket::IsValidSubprotocolString(s));
  }
  for (size_t i = 0; i < 256; ++i) {
    if (std::find(kValidCharacters, kValidCharacters + length,
                  static_cast<char>(i)) != kValidCharacters + length) {
      continue;
    }
    String s;
    s.append(static_cast<UChar>(i));
    EXPECT_FALSE(DOMWebSocket::IsValidSubprotocolString(s));
  }
}

TEST(DOMWebSocketTest, connectSuccess) {
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
  EXPECT_EQ("The message must not be greater than 123 bytes.",
            scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());
}

TEST(DOMWebSocketTest, closeWhenConnecting) {
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
            kWarningMessageLevel, _));
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
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), Send(CString("hello")));
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
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(),
                Send(CString("\xe7\x8b\x90\xe0\xa4\x94")));
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
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  DOMArrayBufferView* view = DOMUint8Array::Create(8);
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), Send(Ref(*view->buffer()), 0, 8));
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
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), Send(CString("hello")));
    EXPECT_CALL(websocket_scope.Channel(), Send(CString("world")));
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
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  {
    InSequence s;
    EXPECT_CALL(websocket_scope.Channel(),
                Connect(KURL("ws://example.com/"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(websocket_scope.Channel(), Send(CString("hello")));
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
  V8TestingScope scope;
  DOMWebSocketTestScope websocket_scope(scope.GetExecutionContext());
  EXPECT_EQ("blob", websocket_scope.Socket().binaryType());

  websocket_scope.Socket().setBinaryType("arraybuffer");

  EXPECT_EQ("arraybuffer", websocket_scope.Socket().binaryType());

  websocket_scope.Socket().setBinaryType("blob");

  EXPECT_EQ("blob", websocket_scope.Socket().binaryType());
}

// FIXME: We should add tests for suspend / resume.

class DOMWebSocketValidClosingTest
    : public testing::TestWithParam<unsigned short> {};

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

INSTANTIATE_TEST_CASE_P(DOMWebSocketValidClosing,
                        DOMWebSocketValidClosingTest,
                        testing::Values(1000, 3000, 3001, 4998, 4999));

class DOMWebSocketInvalidClosingCodeTest
    : public testing::TestWithParam<unsigned short> {};

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
  EXPECT_EQ(String::Format("The code must be either 1000, or between 3000 and "
                           "4999. %d is neither.",
                           GetParam()),
            scope.GetExceptionState().Message());
  EXPECT_EQ(DOMWebSocket::kConnecting, websocket_scope.Socket().readyState());
}

INSTANTIATE_TEST_CASE_P(
    DOMWebSocketInvalidClosingCode,
    DOMWebSocketInvalidClosingCodeTest,
    testing::Values(0, 1, 998, 999, 1001, 2999, 5000, 9999, 65535));

}  // namespace

}  // namespace blink
