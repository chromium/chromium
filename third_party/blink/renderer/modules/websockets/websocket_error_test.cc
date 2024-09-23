// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/websocket_error.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_websocket_close_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_websocket_error.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {

namespace {

class WebSocketErrorTest : public ::testing::Test {
 public:
  // Creates a WebSocketError API from a WebSocketCloseInfo object, optionally
  // with "closeCode" and "reason" attributes set.
  static WebSocketError* CreateError(
      std::optional<uint16_t> close_code = std::nullopt,
      String reason = String(),
      ExceptionState& exception_state = ASSERT_NO_EXCEPTION) {
    auto* close_info = WebSocketCloseInfo::Create();
    if (close_code) {
      close_info->setCloseCode(close_code.value());
    }
    if (!reason.IsNull()) {
      close_info->setReason(reason);
    }
    return WebSocketError::Create("", close_info, exception_state);
  }

 private:
  test::TaskEnvironment task_environment_;
};

TEST_F(WebSocketErrorTest, DefaultConstruct) {
  auto* error = WebSocketError::Create("", WebSocketCloseInfo::Create(),
                                       ASSERT_NO_EXCEPTION);

  ASSERT_TRUE(error);
  EXPECT_EQ(error->message(), "");
  EXPECT_EQ(error->code(), 0);
  EXPECT_EQ(error->closeCode(), std::nullopt);
  EXPECT_EQ(error->reason(), "");
}

TEST_F(WebSocketErrorTest, ConstructWithMessage) {
  auto* error = WebSocketError::Create("hello", WebSocketCloseInfo::Create(),
                                       ASSERT_NO_EXCEPTION);

  ASSERT_TRUE(error);
  EXPECT_EQ(error->message(), "hello");
  EXPECT_EQ(error->code(), 0);
  EXPECT_EQ(error->closeCode(), std::nullopt);
  EXPECT_EQ(error->reason(), "");
}

TEST_F(WebSocketErrorTest, ConstructWithCloseCode) {
  auto* error = CreateError(4011);

  ASSERT_TRUE(error);
  EXPECT_EQ(error->closeCode(), 4011);
  EXPECT_EQ(error->reason(), "");
}

TEST_F(WebSocketErrorTest, ConstructWithReason) {
  auto* error = CreateError(std::nullopt, "wow");

  ASSERT_TRUE(error);
  EXPECT_EQ(error->closeCode(), WebSocketChannel::kCloseEventCodeNormalClosure);
  EXPECT_EQ(error->reason(), "wow");
}

TEST_F(WebSocketErrorTest, ConstructWithEmptyReason) {
  auto* error = CreateError(std::nullopt, "");

  ASSERT_TRUE(error);
  EXPECT_EQ(error->closeCode(), std::nullopt);
  EXPECT_EQ(error->reason(), "");
}

TEST_F(WebSocketErrorTest, ConstructWithInvalidCloseCode) {
  V8TestingScope scope;
  ExceptionState& exception_state = scope.GetExceptionState();
  auto* error = CreateError(1005, String(), exception_state);
  EXPECT_FALSE(error);
  ASSERT_TRUE(exception_state.HadException());
  EXPECT_EQ(
      "The close code must be either 1000, or between 3000 and 4999. 1005 is "
      "neither.",
      exception_state.Message());
  EXPECT_EQ(DOMExceptionCode::kInvalidAccessError,
            DOMExceptionCode{exception_state.Code()});
}

TEST_F(WebSocketErrorTest, ConstructWithOverlongReason) {
  V8TestingScope scope;
  ExceptionState& exception_state = scope.GetExceptionState();
  StringBuilder builder;
  for (int i = 0; i < 32; ++i) {
    // Sparkling Heart emoji. Takes 4 bytes when encoded as unicode.
    builder.Append(UChar32{0x1F496});
  }
  auto* error =
      CreateError(std::nullopt, builder.ReleaseString(), exception_state);
  EXPECT_FALSE(error);
  ASSERT_TRUE(exception_state.HadException());
  EXPECT_EQ("The close reason must not be greater than 123 UTF-8 bytes.",
            exception_state.Message());
  EXPECT_EQ(DOMExceptionCode::kSyntaxError,
            DOMExceptionCode{exception_state.Code()});
}

TEST_F(WebSocketErrorTest, InternalCreate) {
  V8TestingScope scope;
  auto* isolate = scope.GetIsolate();
  auto context = scope.GetContext();
  auto v8value = WebSocketError::Create(isolate, "message", 1000, "reason");

  ASSERT_FALSE(v8value.IsEmpty());
  ASSERT_TRUE(v8value->IsObject());
  v8::Local<v8::Value> stack;
  ASSERT_TRUE(v8value.As<v8::Object>()
                  ->Get(context, V8String(isolate, "stack"))
                  .ToLocal(&stack));
  // Maybe "stack" will return some kind of structured object someday?
  // Explicitly convert it to a string just in case.
  v8::Local<v8::String> stack_as_v8string;
  ASSERT_TRUE(stack->ToString(context).ToLocal(&stack_as_v8string));
  String stack_string = ToCoreString(isolate, stack_as_v8string);
  EXPECT_TRUE(stack_string.Contains("message"));

  WebSocketError* error = V8WebSocketError::ToWrappable(isolate, v8value);
  ASSERT_TRUE(error);
  EXPECT_EQ(error->code(), 0);
  EXPECT_EQ(error->closeCode(), 1000u);
  EXPECT_EQ(error->message(), "message");
  EXPECT_EQ(error->reason(), "reason");
}

}  // namespace

}  // namespace blink
