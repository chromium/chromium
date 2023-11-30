// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

TEST(V8ObjectBuilderTest, addNull) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  V8ObjectBuilder builder(script_state);
  builder.AddNull("null_check");
  ScriptValue json_object = builder.GetScriptValue();
  EXPECT_TRUE(json_object.IsObject());

  String json_string = ToBlinkString<String>(
      scope.GetIsolate(),
      v8::JSON::Stringify(scope.GetContext(),
                          json_object.V8Value().As<v8::Object>())
          .ToLocalChecked(),
      kDoNotExternalize);

  String expected = "{\"null_check\":null}";
  EXPECT_EQ(expected, json_string);
}

TEST(V8ObjectBuilderTest, addBoolean) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  V8ObjectBuilder builder(script_state);
  builder.AddBoolean("b1", true);
  builder.AddBoolean("b2", false);
  ScriptValue json_object = builder.GetScriptValue();
  EXPECT_TRUE(json_object.IsObject());

  String json_string = ToBlinkString<String>(
      scope.GetIsolate(),
      v8::JSON::Stringify(scope.GetContext(),
                          json_object.V8Value().As<v8::Object>())
          .ToLocalChecked(),
      kDoNotExternalize);

  String expected = "{\"b1\":true,\"b2\":false}";
  EXPECT_EQ(expected, json_string);
}

TEST(V8ObjectBuilderTest, addNumber) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  V8ObjectBuilder builder(script_state);
  builder.AddNumber("n1", 123);
  builder.AddNumber("n2", 123.456);
  ScriptValue json_object = builder.GetScriptValue();
  EXPECT_TRUE(json_object.IsObject());

  String json_string = ToBlinkString<String>(
      scope.GetIsolate(),
      v8::JSON::Stringify(scope.GetContext(),
                          json_object.V8Value().As<v8::Object>())
          .ToLocalChecked(),
      kDoNotExternalize);

  String expected = "{\"n1\":123,\"n2\":123.456}";
  EXPECT_EQ(expected, json_string);
}

TEST(V8ObjectBuilderTest, addString) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  V8ObjectBuilder builder(script_state);

  WTF::String test1 = "test1";
  WTF::String test2;
  WTF::String test3 = "test3";
  WTF::String test4;

  builder.AddString("test1", test1);
  builder.AddString("test2", test2);
  builder.AddStringOrNull("test3", test3);
  builder.AddStringOrNull("test4", test4);
  ScriptValue json_object = builder.GetScriptValue();
  EXPECT_TRUE(json_object.IsObject());

  String json_string = ToBlinkString<String>(
      scope.GetIsolate(),
      v8::JSON::Stringify(scope.GetContext(),
                          json_object.V8Value().As<v8::Object>())
          .ToLocalChecked(),
      kDoNotExternalize);

  String expected =
      "{\"test1\":\"test1\",\"test2\":\"\",\"test3\":\"test3\",\"test4\":"
      "null}";
  EXPECT_EQ(expected, json_string);
}

TEST(V8ObjectBuilderTest, add) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  V8ObjectBuilder builder(script_state);
  V8ObjectBuilder result(script_state);
  builder.AddNumber("n1", 123);
  builder.AddNumber("n2", 123.456);
  result.Add("builder", builder);
  ScriptValue builder_json_object = builder.GetScriptValue();
  ScriptValue result_json_object = result.GetScriptValue();
  EXPECT_TRUE(builder_json_object.IsObject());
  EXPECT_TRUE(result_json_object.IsObject());

  String json_string = ToBlinkString<String>(
      scope.GetIsolate(),
      v8::JSON::Stringify(scope.GetContext(),
                          result_json_object.V8Value().As<v8::Object>())
          .ToLocalChecked(),
      kDoNotExternalize);

  String expected = "{\"builder\":{\"n1\":123,\"n2\":123.456}}";
  EXPECT_EQ(expected, json_string);
}

}  // namespace

}  // namespace blink
