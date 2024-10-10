// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/web_transport_error.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_error.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_error_init.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_error.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(WebTransportErrorTest, DefaultConstruct) {
  test::TaskEnvironment task_environment;
  auto* error = WebTransportError::Create(WebTransportErrorInit::Create());

  EXPECT_EQ(error->code(), 0);
  EXPECT_EQ(error->streamErrorCode(), std::nullopt);
  EXPECT_EQ(error->message(), "");
  EXPECT_EQ(error->source(), "stream");
}

TEST(WebTransportErrorTest, ConstructWithStreamErrorCode) {
  test::TaskEnvironment task_environment;
  auto* init = WebTransportErrorInit::Create();
  init->setStreamErrorCode(11);
  auto* error = WebTransportError::Create(init);

  ASSERT_TRUE(error->streamErrorCode().has_value());
  EXPECT_EQ(error->streamErrorCode().value(), 11u);
}

TEST(WebTransportErrorTest, ConstructWithMessage) {
  test::TaskEnvironment task_environment;
  auto* init = WebTransportErrorInit::Create();
  init->setMessage("wow");
  auto* error = WebTransportError::Create(init);

  EXPECT_EQ(error->message(), "wow");
}

TEST(WebTransportErrorTest, InternalCreate) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* isolate = scope.GetIsolate();
  auto context = scope.GetContext();
  auto v8value = WebTransportError::Create(
      isolate, 27, "badness", V8WebTransportErrorSource::Enum::kSession);

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
  EXPECT_TRUE(stack_string.Contains("badness"));

  WebTransportError* error = V8WebTransportError::ToWrappable(isolate, v8value);
  ASSERT_TRUE(error);
  EXPECT_EQ(error->code(), 0);
  ASSERT_TRUE(error->streamErrorCode().has_value());
  EXPECT_EQ(error->streamErrorCode().value(), 27u);
  EXPECT_EQ(error->message(), "badness");
  EXPECT_EQ(error->source(), "session");
}

}  // namespace blink
