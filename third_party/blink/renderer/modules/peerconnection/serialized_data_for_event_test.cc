// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/serialized_data_for_event.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/v8_script_value_serializer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "v8/include/v8.h"

namespace blink {

TEST(SerializedDataForEventTest, DataTypeNull) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  SerializedDataForEvent* serialized_data =
      MakeGarbageCollected<SerializedDataForEvent>(nullptr);
  ScriptValue value = serialized_data->Deserialize(script_state);
  EXPECT_TRUE(value.IsNull());
}

TEST(SerializedDataForEventTest, DataTypeSerializedScriptValueAsBoolean) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;

  v8::Local<v8::Value> v8_original_true = v8::True(v8_scope.GetIsolate());
  ScriptState* script_state = v8_scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> serialized_script_value =
      SerializedScriptValue::Serialize(
          v8_scope.GetIsolate(), v8_original_true,
          SerializedScriptValue::SerializeOptions(), ASSERT_NO_EXCEPTION);

  SerializedDataForEvent* serialized_data =
      MakeGarbageCollected<SerializedDataForEvent>(serialized_script_value);
  ScriptValue value = serialized_data->Deserialize(script_state);

  EXPECT_EQ(value.V8Value(), v8_original_true);
}

TEST(SerializedDataForEventTest, DataTypeSerializedScriptValueAsNumber) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  static const double kTestNumber = 2.34;
  v8::Local<v8::Value> v8_number =
      v8::Number::New(v8_scope.GetIsolate(), kTestNumber);
  scoped_refptr<SerializedScriptValue> serialized_script_value =
      SerializedScriptValue::Serialize(
          v8_scope.GetIsolate(), v8_number,
          SerializedScriptValue::SerializeOptions(), ASSERT_NO_EXCEPTION);
  SerializedDataForEvent* serialized_data_number =
      MakeGarbageCollected<SerializedDataForEvent>(serialized_script_value);
  ScriptValue value = serialized_data_number->Deserialize(script_state);
  EXPECT_EQ(value.V8Value().As<v8::Number>()->Value(), kTestNumber);
}

}  // namespace blink
