// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most testing for WebSocketStream is done via web platform tests. These unit
// tests just cover the most common functionality.

#include "third_party/blink/renderer/modules/websockets/websocket_stream.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_websocket_close_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_websocket_error.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_websocket_stream_options.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/websockets/mock_websocket_channel.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Return;

typedef testing::StrictMock<testing::MockFunction<void(int)>>
    Checkpoint;  // NOLINT

class WebSocketStreamTest : public ::testing::Test {
 public:
  WebSocketStreamTest()
      : channel_(MakeGarbageCollected<MockWebSocketChannel>()) {}

  void TearDown() override {
    testing::Mock::VerifyAndClear(channel_);
    channel_ = nullptr;
  }

  // Returns a reference for easy use with EXPECT_CALL(Channel(), ...).
  MockWebSocketChannel& Channel() const { return *channel_; }

  WebSocketStream* Create(ScriptState* script_state,
                          const String& url,
                          ExceptionState& exception_state) {
    return Create(script_state, url, WebSocketStreamOptions::Create(),
                  exception_state);
  }

  WebSocketStream* Create(ScriptState* script_state,
                          const String& url,
                          WebSocketStreamOptions* options,
                          ExceptionState& exception_state) {
    return WebSocketStream::CreateForTesting(script_state, url, options,
                                             channel_, exception_state);
  }

  bool IsDOMException(ScriptState* script_state,
                      ScriptValue value,
                      DOMExceptionCode code) {
    auto* dom_exception = V8DOMException::ToWrappable(
        script_state->GetIsolate(), value.V8Value());
    if (!dom_exception)
      return false;

    return dom_exception->code() == static_cast<uint16_t>(code);
  }

  bool IsWebSocketError(ScriptState* script_state, ScriptValue value) {
    return V8WebSocketError::HasInstance(script_state->GetIsolate(),
                                         value.V8Value());
  }

  // Returns the value of the property |key| on object |object|, stringified as
  // a UTF-8 encoded std::string so that it can be compared and printed by
  // EXPECT_EQ. |object| must have been verified to be a v8::Object. |key| must
  // be encoded as latin1. undefined and null values are stringified as
  // "undefined" and "null" respectively. "undefined" is also used to mean "not
  // found".
  std::string PropertyAsString(ScriptState* script_state,
                               v8::Local<v8::Value> object,
                               String key) {
    v8::Local<v8::Value> value;
    auto* isolate = script_state->GetIsolate();
    if (!object.As<v8::Object>()
             ->GetRealNamedProperty(script_state->GetContext(),
                                    V8String(isolate, key))
             .ToLocal(&value)) {
      value = v8::Undefined(isolate);
    }

    v8::String::Utf8Value utf8value(isolate, value);
    return std::string(*utf8value, utf8value.length());
  }

 private:
  test::TaskEnvironment task_environment_;
  Persistent<MockWebSocketChannel> channel_;
};

TEST_F(WebSocketStreamTest, ConstructWithBadURL) {
  V8TestingScope scope;
  auto& exception_state = scope.GetExceptionState();

  EXPECT_CALL(Channel(), ApplyBackpressure());

  auto* stream = Create(scope.GetScriptState(), "bad-scheme:", exception_state);

  EXPECT_FALSE(stream);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(DOMExceptionCode::kSyntaxError,
            exception_state.CodeAs<DOMExceptionCode>());
  EXPECT_EQ(
      "The URL's scheme must be either 'http', 'https', 'ws', or 'wss'. "
      "'bad-scheme' is not allowed.",
      exception_state.Message());
}

// Most coverage for bad constructor arguments is provided by
// dom_websocket_test.cc.
// TODO(ricea): Should we duplicate those tests here?

TEST_F(WebSocketStreamTest, Connect) {
  V8TestingScope scope;

  {
    InSequence s;
    EXPECT_CALL(Channel(), ApplyBackpressure());
    EXPECT_CALL(Channel(), Connect(KURL("ws://example.com/hoge"), String()))
        .WillOnce(Return(true));
  }

  auto* stream = Create(scope.GetScriptState(), "ws://example.com/hoge",
                        ASSERT_NO_EXCEPTION);

  ASSERT_TRUE(stream);
  EXPECT_EQ(KURL("ws://example.com/hoge"), stream->url());
}

TEST_F(WebSocketStreamTest, ConnectWithProtocols) {
  V8TestingScope scope;

  {
    InSequence s;
    EXPECT_CALL(Channel(), ApplyBackpressure());
    EXPECT_CALL(Channel(),
                Connect(KURL("ws://example.com/chat"), String("chat0, chat1")))
        .WillOnce(Return(true));
  }

  auto* options = WebSocketStreamOptions::Create();
  options->setProtocols({"chat0", "chat1"});
  auto* stream = Create(scope.GetScriptState(), "ws://example.com/chat",
                        options, ASSERT_NO_EXCEPTION);

  ASSERT_TRUE(stream);
  EXPECT_EQ(KURL("ws://example.com/chat"), stream->url());
}

TEST_F(WebSocketStreamTest, ConnectWithFailedHandshake) {
  V8TestingScope scope;

  {
    InSequence s;
    EXPECT_CALL(Channel(), ApplyBackpressure());
    EXPECT_CALL(Channel(), Connect(KURL("ws://example.com/chat"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(Channel(), Disconnect());
  }

  auto* script_state = scope.GetScriptState();
  auto* stream =
      Create(script_state, "ws://example.com/chat", ASSERT_NO_EXCEPTION);

  ASSERT_TRUE(stream);
  EXPECT_EQ(KURL("ws://example.com/chat"), stream->url());

  ScriptPromiseTester opened_tester(script_state, stream->opened(script_state));
  ScriptPromiseTester closed_tester(script_state, stream->closed(script_state));

  stream->DidError();
  stream->DidClose(WebSocketChannelClient::kClosingHandshakeIncomplete,
                   WebSocketChannel::kCloseEventCodeAbnormalClosure, String());

  opened_tester.WaitUntilSettled();
  closed_tester.WaitUntilSettled();

  EXPECT_TRUE(opened_tester.IsRejected());
  EXPECT_TRUE(IsWebSocketError(script_state, opened_tester.Value()));

  EXPECT_TRUE(closed_tester.IsRejected());
  EXPECT_TRUE(IsWebSocketError(script_state, closed_tester.Value()));
}

TEST_F(WebSocketStreamTest, ConnectWithSuccessfulHandshake) {
  V8TestingScope scope;

  {
    InSequence s;
    EXPECT_CALL(Channel(), ApplyBackpressure());
    EXPECT_CALL(Channel(),
                Connect(KURL("ws://example.com/chat"), String("chat")))
        .WillOnce(Return(true));
  }

  auto* options = WebSocketStreamOptions::Create();
  options->setProtocols({"chat"});
  auto* script_state = scope.GetScriptState();
  auto* stream = Create(script_state, "ws://example.com/chat", options,
                        ASSERT_NO_EXCEPTION);

  ASSERT_TRUE(stream);
  EXPECT_EQ(KURL("ws://example.com/chat"), stream->url());

  ScriptPromiseTester opened_tester(script_state, stream->opened(script_state));

  stream->DidConnect("chat", "permessage-deflate");

  opened_tester.WaitUntilSettled();

  EXPECT_TRUE(opened_tester.IsFulfilled());
  v8::Local<v8::Value> value = opened_tester.Value().V8Value();
  ASSERT_FALSE(value.IsEmpty());
  ASSERT_TRUE(value->IsObject());
  EXPECT_EQ(PropertyAsString(script_state, value, "readable"),
            "[object ReadableStream]");
  EXPECT_EQ(PropertyAsString(script_state, value, "writable"),
            "[object WritableStream]");
  EXPECT_EQ(PropertyAsString(script_state, value, "protocol"), "chat");
  EXPECT_EQ(PropertyAsString(script_state, value, "extensions"),
            "permessage-deflate");
}

TEST_F(WebSocketStreamTest, ConnectThenCloseCleanly) {
  V8TestingScope scope;

  {
    InSequence s;
    EXPECT_CALL(Channel(), ApplyBackpressure());
    EXPECT_CALL(Channel(), Connect(KURL("ws://example.com/echo"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(Channel(), Close(-1, String("")));
    EXPECT_CALL(Channel(), Disconnect());
  }

  auto* script_state = scope.GetScriptState();
  auto* stream =
      Create(script_state, "ws://example.com/echo", ASSERT_NO_EXCEPTION);

  ASSERT_TRUE(stream);

  stream->DidConnect("", "");

  ScriptPromiseTester closed_tester(script_state, stream->closed(script_state));

  stream->close(MakeGarbageCollected<WebSocketCloseInfo>(),
                scope.GetExceptionState());
  stream->DidClose(WebSocketChannelClient::kClosingHandshakeComplete, 1005, "");

  closed_tester.WaitUntilSettled();
  EXPECT_TRUE(closed_tester.IsFulfilled());
  ASSERT_TRUE(closed_tester.Value().IsObject());
  EXPECT_EQ(PropertyAsString(script_state, closed_tester.Value().V8Value(),
                             "closeCode"),
            "1005");
  EXPECT_EQ(
      PropertyAsString(script_state, closed_tester.Value().V8Value(), "reason"),
      "");
}

TEST_F(WebSocketStreamTest, CloseDuringHandshake) {
  V8TestingScope scope;

  {
    InSequence s;
    EXPECT_CALL(Channel(), ApplyBackpressure());
    EXPECT_CALL(Channel(), Connect(KURL("ws://example.com/echo"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(
        Channel(),
        FailMock(
            String("WebSocket is closed before the connection is established."),
            mojom::ConsoleMessageLevel::kWarning, _));
    EXPECT_CALL(Channel(), Disconnect());
  }

  auto* script_state = scope.GetScriptState();
  auto* stream =
      Create(script_state, "ws://example.com/echo", ASSERT_NO_EXCEPTION);

  ASSERT_TRUE(stream);

  ScriptPromiseTester opened_tester(script_state, stream->opened(script_state));
  ScriptPromiseTester closed_tester(script_state, stream->closed(script_state));

  stream->close(MakeGarbageCollected<WebSocketCloseInfo>(),
                scope.GetExceptionState());
  stream->DidClose(WebSocketChannelClient::kClosingHandshakeIncomplete, 1006,
                   "");

  opened_tester.WaitUntilSettled();
  closed_tester.WaitUntilSettled();

  EXPECT_TRUE(opened_tester.IsRejected());
  EXPECT_TRUE(IsWebSocketError(script_state, opened_tester.Value()));
  EXPECT_TRUE(closed_tester.IsRejected());
  EXPECT_TRUE(IsWebSocketError(script_state, closed_tester.Value()));
}

TEST_F(WebSocketStreamTest, AbortBeforeHandshake) {
  V8TestingScope scope;

  // ApplyBackpressure() is currently called in this case but doesn't have to
  // be.
  EXPECT_CALL(Channel(), ApplyBackpressure()).Times(AnyNumber());

  auto* script_state = scope.GetScriptState();

  auto* options = WebSocketStreamOptions::Create();
  options->setSignal(AbortSignal::abort(script_state));

  auto* stream = Create(script_state, "ws://example.com/echo", options,
                        ASSERT_NO_EXCEPTION);

  ASSERT_TRUE(stream);

  ScriptPromiseTester opened_tester(script_state, stream->opened(script_state));
  ScriptPromiseTester closed_tester(script_state, stream->closed(script_state));

  opened_tester.WaitUntilSettled();
  closed_tester.WaitUntilSettled();

  EXPECT_TRUE(opened_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(script_state, opened_tester.Value(),
                             DOMExceptionCode::kAbortError));
  EXPECT_TRUE(closed_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(script_state, closed_tester.Value(),
                             DOMExceptionCode::kAbortError));
}

TEST_F(WebSocketStreamTest, AbortDuringHandshake) {
  V8TestingScope scope;

  {
    InSequence s;
    EXPECT_CALL(Channel(), ApplyBackpressure());
    EXPECT_CALL(Channel(), Connect(KURL("ws://example.com/echo"), String()))
        .WillOnce(Return(true));
    EXPECT_CALL(Channel(), CancelHandshake());
  }

  auto* script_state = scope.GetScriptState();

  auto* controller = AbortController::Create(script_state);
  auto* options = WebSocketStreamOptions::Create();
  options->setSignal(controller->signal());

  auto* stream = Create(script_state, "ws://example.com/echo", options,
                        ASSERT_NO_EXCEPTION);

  ASSERT_TRUE(stream);

  ScriptPromiseTester opened_tester(script_state, stream->opened(script_state));
  ScriptPromiseTester closed_tester(script_state, stream->closed(script_state));

  controller->abort(script_state);

  opened_tester.WaitUntilSettled();
  closed_tester.WaitUntilSettled();

  EXPECT_TRUE(opened_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(script_state, opened_tester.Value(),
                             DOMExceptionCode::kAbortError));
  EXPECT_TRUE(closed_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(script_state, closed_tester.Value(),
                             DOMExceptionCode::kAbortError));
}

// Aborting after the handshake is complete does nothing.
TEST_F(WebSocketStreamTest, AbortAfterHandshake) {
  V8TestingScope scope;

  {
    InSequence s;
    EXPECT_CALL(Channel(), ApplyBackpressure());
    EXPECT_CALL(Channel(), Connect(KURL("ws://example.com/echo"), String()))
        .WillOnce(Return(true));
  }

  auto* script_state = scope.GetScriptState();

  auto* controller = AbortController::Create(script_state);
  auto* options = WebSocketStreamOptions::Create();
  options->setSignal(controller->signal());

  auto* stream = Create(script_state, "ws://example.com/echo", options,
                        ASSERT_NO_EXCEPTION);

  ASSERT_TRUE(stream);

  ScriptPromiseTester opened_tester(script_state, stream->opened(script_state));
  ScriptPromiseTester closed_tester(script_state, stream->closed(script_state));

  stream->DidConnect("", "permessage-deflate");

  opened_tester.WaitUntilSettled();
  EXPECT_TRUE(opened_tester.IsFulfilled());

  // This should do nothing.
  controller->abort(script_state);

  test::RunPendingTasks();

  EXPECT_FALSE(closed_tester.IsFulfilled());
  EXPECT_FALSE(closed_tester.IsRejected());
}

}  // namespace

}  // namespace blink
