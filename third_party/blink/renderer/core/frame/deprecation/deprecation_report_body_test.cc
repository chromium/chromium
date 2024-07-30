// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/deprecation/deprecation_report_body.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

TEST(DeprecationReportBodyJSONTest, noAnticipatedRemoval) {
  test::TaskEnvironment task_environment;
  DeprecationReportBody* body = MakeGarbageCollected<DeprecationReportBody>(
      "test_id", std::nullopt, "test_message");
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  V8ObjectBuilder builder(script_state);
  body->BuildJSONValue(builder);
  ScriptValue json_object = builder.GetScriptValue();
  EXPECT_TRUE(json_object.IsObject());

  String json_string = ToBlinkString<String>(
      scope.GetIsolate(),
      v8::JSON::Stringify(scope.GetContext(),
                          json_object.V8Value().As<v8::Object>())
          .ToLocalChecked(),
      kDoNotExternalize);

  String expected =
      "{\"sourceFile\":null,\"lineNumber\":null,\"columnNumber\":null,\"id\":"
      "\"test_id\",\"message\":\"test_message\",\"anticipatedRemoval\":null}";
  EXPECT_EQ(expected, json_string);
}

TEST(DeprecationReportBodyJSONTest, actualAnticipatedRemoval) {
  test::TaskEnvironment task_environment;
  DeprecationReportBody* body = MakeGarbageCollected<DeprecationReportBody>(
      "test_id", base::Time::FromMillisecondsSinceUnixEpoch(1575950400000),
      "test_message");
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  V8ObjectBuilder builder(script_state);
  body->BuildJSONValue(builder);
  ScriptValue json_object = builder.GetScriptValue();
  EXPECT_TRUE(json_object.IsObject());

  String json_string = ToBlinkString<String>(
      scope.GetIsolate(),
      v8::JSON::Stringify(scope.GetContext(),
                          json_object.V8Value().As<v8::Object>())
          .ToLocalChecked(),
      kDoNotExternalize);

  String expected =
      "{\"sourceFile\":null,\"lineNumber\":null,\"columnNumber\":null,\"id\":"
      "\"test_id\",\"message\":\"test_message\",\"anticipatedRemoval\":\"2019-"
      "12-10T04:00:00.000Z\"}";
  EXPECT_EQ(expected, json_string);
}

}  // namespace

}  // namespace blink
