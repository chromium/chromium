// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_script_transformer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/workers/custom_event_message.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_script_transform.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8-primitive.h"

namespace blink {

TEST(RTCRtpScriptTransformerTest, OptionsAsBoolean) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  v8::Local<v8::Value> v8_original_true = v8::True(v8_scope.GetIsolate());
  scoped_refptr<SerializedScriptValue> serialized_script_value =
      SerializedScriptValue::Serialize(
          v8_scope.GetIsolate(), v8_original_true,
          SerializedScriptValue::SerializeOptions(), ASSERT_NO_EXCEPTION);
  CustomEventMessage options;
  options.message = serialized_script_value;
  RTCRtpScriptTransformer* transformer =
      MakeGarbageCollected<RTCRtpScriptTransformer>(
          script_state, std::move(options), /*transform_task_runner=*/nullptr,
          CrossThreadWeakHandle<RTCRtpScriptTransform>(nullptr));
  EXPECT_EQ(transformer->options(script_state).V8Value(), v8_original_true);
}

TEST(RTCRtpScriptTransformerTest, OptionsAsNumber) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  const double kNumber = 2.34;
  v8::Local<v8::Value> v8_number =
      v8::Number::New(v8_scope.GetIsolate(), kNumber);
  scoped_refptr<SerializedScriptValue> serialized_script_value =
      SerializedScriptValue::Serialize(
          v8_scope.GetIsolate(), v8_number,
          SerializedScriptValue::SerializeOptions(), ASSERT_NO_EXCEPTION);
  CustomEventMessage options;
  options.message = serialized_script_value;
  RTCRtpScriptTransformer* transformer =
      MakeGarbageCollected<RTCRtpScriptTransformer>(
          script_state, std::move(options), /*transform_task_runner=*/nullptr,
          CrossThreadWeakHandle<RTCRtpScriptTransform>(nullptr));
  EXPECT_EQ(
      transformer->options(script_state).V8Value().As<v8::Number>()->Value(),
      kNumber);
}

TEST(RTCRtpScriptTransformerTest, OptionsAsNull) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  v8::Local<v8::Value> v8_null = v8::Null(v8_scope.GetIsolate());
  scoped_refptr<SerializedScriptValue> serialized_script_value =
      SerializedScriptValue::Serialize(
          v8_scope.GetIsolate(), v8_null,
          SerializedScriptValue::SerializeOptions(), ASSERT_NO_EXCEPTION);
  CustomEventMessage options;
  options.message = std::move(serialized_script_value);
  RTCRtpScriptTransformer* transformer =
      MakeGarbageCollected<RTCRtpScriptTransformer>(
          script_state, std::move(options), /*transform_task_runner=*/nullptr,
          CrossThreadWeakHandle<RTCRtpScriptTransform>(nullptr));
  EXPECT_EQ(transformer->options(script_state).V8Value(), v8_null);
}

}  // namespace blink
