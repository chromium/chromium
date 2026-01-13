// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_tool_call_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_tool_error_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_tool_result_content.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_tool_result_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_tool_success_init.h"
#include "third_party/blink/renderer/modules/ai/language_model_tool_call.h"
#include "third_party/blink/renderer/modules/ai/language_model_tool_error.h"
#include "third_party/blink/renderer/modules/ai/language_model_tool_success.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

TEST(LanguageModelToolTypeTest, ToolCallConstructorAndAttributes) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* init = LanguageModelToolCallInit::Create(scope.GetIsolate());
  init->setCallID("call-123");
  init->setName("get_weather");

  // Create arguments object.
  v8::Local<v8::Object> args = v8::Object::New(scope.GetIsolate());
  args->Set(scope.GetContext(),
            v8::String::NewFromUtf8Literal(scope.GetIsolate(), "location"),
            v8::String::NewFromUtf8Literal(scope.GetIsolate(), "Seattle"))
      .Check();
  init->setArguments(blink::ScriptObject(scope.GetIsolate(), args));

  DummyExceptionStateForTesting exception_state;
  auto* tool_call = LanguageModelToolCall::Create(init, exception_state);

  ASSERT_FALSE(exception_state.HadException());
  ASSERT_NE(tool_call, nullptr);
  EXPECT_EQ(tool_call->callID(), "call-123");
  EXPECT_EQ(tool_call->name(), "get_weather");

  // Verify arguments can be retrieved and check content.
  v8::Local<v8::Value> args_value =
      tool_call->arguments(scope.GetScriptState());
  ASSERT_FALSE(args_value.IsEmpty());
  EXPECT_TRUE(args_value->IsObject());

  // Check the "location" property value.
  v8::Local<v8::Object> args_obj = args_value.As<v8::Object>();
  v8::Local<v8::Value> location_value;
  ASSERT_TRUE(args_obj
                  ->Get(scope.GetContext(), v8::String::NewFromUtf8Literal(
                                                scope.GetIsolate(), "location"))
                  .ToLocal(&location_value));
  EXPECT_TRUE(location_value->IsString());
  v8::String::Utf8Value location_str(scope.GetIsolate(), location_value);
  EXPECT_STREQ(*location_str, "Seattle");
}

TEST(LanguageModelToolTypeTest, ToolCallWithoutArguments) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* init = LanguageModelToolCallInit::Create(scope.GetIsolate());
  init->setCallID("call-456");
  init->setName("get_time");
  // No arguments set.

  DummyExceptionStateForTesting exception_state;
  auto* tool_call = LanguageModelToolCall::Create(init, exception_state);

  ASSERT_FALSE(exception_state.HadException());
  ASSERT_NE(tool_call, nullptr);
  EXPECT_EQ(tool_call->callID(), "call-456");
  EXPECT_EQ(tool_call->name(), "get_time");

  // Verify arguments is null.
  v8::Local<v8::Value> args_value =
      tool_call->arguments(scope.GetScriptState());
  EXPECT_TRUE(args_value->IsNull());
}

TEST(LanguageModelToolTypeTest, ToolSuccessConstructorAndAttributes) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* init = LanguageModelToolSuccessInit::Create(scope.GetIsolate());
  init->setCallID("call-123");
  init->setName("get_weather");

  // Create result array with a text result.
  HeapVector<Member<LanguageModelToolResultContent>> result_items;
  auto* result_item =
      LanguageModelToolResultContent::Create(scope.GetIsolate());
  result_item->setType(V8LanguageModelToolResultType::Enum::kText);

  // Set value as a simple V8 string.
  v8::Local<v8::String> text_value =
      v8::String::NewFromUtf8Literal(scope.GetIsolate(), "Sunny, 72°F");
  result_item->setValue(ScriptValue(scope.GetIsolate(), text_value));
  result_items.push_back(result_item);
  init->setResult(result_items);

  DummyExceptionStateForTesting exception_state;
  auto* tool_success = LanguageModelToolSuccess::Create(init, exception_state);

  ASSERT_FALSE(exception_state.HadException());
  ASSERT_NE(tool_success, nullptr);
  EXPECT_EQ(tool_success->callID(), "call-123");
  EXPECT_EQ(tool_success->name(), "get_weather");
  EXPECT_EQ(tool_success->result().size(), 1u);
  EXPECT_EQ(tool_success->result()[0]->type().AsString(), "text");

  // Check the text value content.
  v8::Local<v8::Value> value_v8 = tool_success->result()[0]->value().V8Value();
  EXPECT_TRUE(value_v8->IsString());
  v8::String::Utf8Value value_str(scope.GetIsolate(), value_v8);
  EXPECT_STREQ(*value_str, "Sunny, 72°F");
}

TEST(LanguageModelToolTypeTest, ToolSuccessWithMultipleResults) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* init = LanguageModelToolSuccessInit::Create(scope.GetIsolate());
  init->setCallID("call-789");
  init->setName("search");

  // Create multiple result items.
  HeapVector<Member<LanguageModelToolResultContent>> result_items;

  auto* text_result =
      LanguageModelToolResultContent::Create(scope.GetIsolate());
  text_result->setType(V8LanguageModelToolResultType::Enum::kText);
  v8::Local<v8::String> text_value =
      v8::String::NewFromUtf8Literal(scope.GetIsolate(), "Result 1");
  text_result->setValue(ScriptValue(scope.GetIsolate(), text_value));
  result_items.push_back(text_result);

  auto* object_result =
      LanguageModelToolResultContent::Create(scope.GetIsolate());
  object_result->setType(V8LanguageModelToolResultType::Enum::kObject);
  v8::Local<v8::Object> obj = v8::Object::New(scope.GetIsolate());
  obj->Set(scope.GetContext(),
           v8::String::NewFromUtf8Literal(scope.GetIsolate(), "score"),
           v8::Number::New(scope.GetIsolate(), 0.95))
      .Check();
  object_result->setValue(ScriptValue(scope.GetIsolate(), obj));
  result_items.push_back(object_result);

  init->setResult(result_items);

  DummyExceptionStateForTesting exception_state;
  auto* tool_success = LanguageModelToolSuccess::Create(init, exception_state);

  ASSERT_FALSE(exception_state.HadException());
  ASSERT_NE(tool_success, nullptr);
  EXPECT_EQ(tool_success->result().size(), 2u);
  EXPECT_EQ(tool_success->result()[0]->type().AsString(), "text");
  EXPECT_EQ(tool_success->result()[1]->type().AsString(), "object");
}

TEST(LanguageModelToolTypeTest, ToolErrorConstructorAndAttributes) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* init = LanguageModelToolErrorInit::Create(scope.GetIsolate());
  init->setCallID("call-error");
  init->setName("broken_tool");
  init->setErrorMessage("Tool execution failed: timeout");

  DummyExceptionStateForTesting exception_state;
  auto* tool_error = LanguageModelToolError::Create(init, exception_state);

  ASSERT_FALSE(exception_state.HadException());
  ASSERT_NE(tool_error, nullptr);
  EXPECT_EQ(tool_error->callID(), "call-error");
  EXPECT_EQ(tool_error->name(), "broken_tool");
  EXPECT_EQ(tool_error->errorMessage(), "Tool execution failed: timeout");
}

}  // namespace blink
