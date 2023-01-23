// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_measure.h"

#include "third_party/blink/public/mojom/timing/performance_mark_or_measure.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

PerformanceMeasure::PerformanceMeasure(
    ScriptState* script_state,
    const AtomicString& name,
    double start_time,
    double end_time,
    scoped_refptr<SerializedScriptValue> serialized_detail,
    ExceptionState& exception_state,
    DOMWindow* source)
    : PerformanceEntry(name, start_time, end_time, source),
      serialized_detail_(serialized_detail) {}

// static
PerformanceMeasure* PerformanceMeasure::Create(ScriptState* script_state,
                                               const AtomicString& name,
                                               double start_time,
                                               double end_time,
                                               const ScriptValue& detail,
                                               ExceptionState& exception_state,
                                               DOMWindow* source) {
  scoped_refptr<SerializedScriptValue> serialized_detail;
  if (detail.IsEmpty()) {
    serialized_detail = nullptr;
  } else {
    serialized_detail = SerializedScriptValue::Serialize(
        script_state->GetIsolate(), detail.V8Value(),
        SerializedScriptValue::SerializeOptions(), exception_state);
    if (exception_state.HadException())
      return nullptr;
  }
  return MakeGarbageCollected<PerformanceMeasure>(
      script_state, name, start_time, end_time, serialized_detail,
      exception_state, source);
}

ScriptValue PerformanceMeasure::detail(ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (!serialized_detail_)
    return ScriptValue(isolate, v8::Null(isolate));
  auto result = deserialized_detail_map_.insert(
      script_state, TraceWrapperV8Reference<v8::Value>());
  TraceWrapperV8Reference<v8::Value>& relevant_data =
      result.stored_value->value;
  if (!result.is_new_entry)
    return ScriptValue(isolate, relevant_data.Get(isolate));
  v8::Local<v8::Value> value = serialized_detail_->Deserialize(isolate);
  relevant_data.Reset(isolate, value);
  return ScriptValue(isolate, value);
}

const AtomicString& PerformanceMeasure::entryType() const {
  return performance_entry_names::kMeasure;
}

PerformanceEntryType PerformanceMeasure::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kMeasure;
}

mojom::blink::PerformanceMarkOrMeasurePtr
PerformanceMeasure::ToMojoPerformanceMarkOrMeasure() {
  auto mojo_performance_mark_or_measure =
      PerformanceEntry::ToMojoPerformanceMarkOrMeasure();
  mojo_performance_mark_or_measure->detail = serialized_detail_->GetWireData();
  return mojo_performance_mark_or_measure;
}

void PerformanceMeasure::Trace(Visitor* visitor) const {
  visitor->Trace(deserialized_detail_map_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
