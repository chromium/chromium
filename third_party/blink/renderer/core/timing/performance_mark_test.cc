// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_mark.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/performance_mark_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

TEST(PerformanceMarkTest, CreateWithScriptValue) {
  V8TestingScope scope;

  ExceptionState& exception_state = scope.GetExceptionState();
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();
  scoped_refptr<SerializedScriptValue> payload_string =
      SerializedScriptValue::Create(String("some-payload"));
  ScriptValue script_value(isolate, payload_string->Deserialize(isolate));

  PerformanceMark* pm = PerformanceMark::Create(script_state, "mark-name",
                                                /*start_time=*/0.0,
                                                script_value, exception_state);

  ASSERT_EQ(pm->entryType(), performance_entry_names::kMark);
  ASSERT_EQ(pm->EntryTypeEnum(), PerformanceEntry::EntryType::kMark);
  ASSERT_EQ(payload_string->Deserialize(isolate),
            pm->detail(script_state).V8Value());
}

TEST(PerformanceMarkTest, CreateWithOptions) {
  V8TestingScope scope;

  ExceptionState& exception_state = scope.GetExceptionState();
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();
  scoped_refptr<SerializedScriptValue> payload_string =
      SerializedScriptValue::Create(String("some-payload"));
  ScriptValue script_value(isolate, payload_string->Deserialize(isolate));

  PerformanceMarkOptions* options = PerformanceMarkOptions::Create();
  options->setDetail(script_value);

  PerformanceMark* pm = PerformanceMark::Create(script_state, "mark-name",
                                                options, exception_state);

  ASSERT_EQ(pm->entryType(), performance_entry_names::kMark);
  ASSERT_EQ(pm->EntryTypeEnum(), PerformanceEntry::EntryType::kMark);
  ASSERT_EQ(payload_string->Deserialize(isolate),
            pm->detail(script_state).V8Value());
}

TEST(PerformanceMarkTest, Construction) {
  V8TestingScope scope;

  ExceptionState& exception_state = scope.GetExceptionState();
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();

  PerformanceMark* pm = MakeGarbageCollected<PerformanceMark>(
      "mark-name", 0, SerializedScriptValue::NullValue(), exception_state);
  ASSERT_EQ(pm->entryType(), performance_entry_names::kMark);
  ASSERT_EQ(pm->EntryTypeEnum(), PerformanceEntry::EntryType::kMark);

  ASSERT_EQ(SerializedScriptValue::NullValue()->Deserialize(isolate),
            pm->detail(script_state).V8Value());
}

TEST(PerformanceMarkTest, ConstructionWithDetail) {
  V8TestingScope scope;

  ExceptionState& exception_state = scope.GetExceptionState();
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();
  scoped_refptr<SerializedScriptValue> payload_string =
      SerializedScriptValue::Create(String("some-payload"));

  PerformanceMark* pm = MakeGarbageCollected<PerformanceMark>(
      "mark-name", 0, payload_string, exception_state);
  ASSERT_EQ(pm->entryType(), performance_entry_names::kMark);
  ASSERT_EQ(pm->EntryTypeEnum(), PerformanceEntry::EntryType::kMark);

  ASSERT_EQ(payload_string->Deserialize(isolate),
            pm->detail(script_state).V8Value());
}

}  // namespace blink
