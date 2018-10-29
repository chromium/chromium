// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_measure.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"

namespace blink {

PerformanceMeasure::PerformanceMeasure(ScriptState* script_state,
                                       const AtomicString& name,
                                       double start_time,
                                       double end_time,
                                       const ScriptValue& detail)
    : PerformanceEntry(name, start_time, end_time) {
  if (detail.IsEmpty()) {
    detail_ = SerializedScriptValue::NullValue();
  } else {
    detail_ = SerializedScriptValue::SerializeAndSwallowExceptions(
        script_state->GetIsolate(), detail.V8Value());
  }
}

ScriptValue PerformanceMeasure::detail(ScriptState* script_state) const {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Value> detail = detail_->Deserialize(isolate);
  return ScriptValue(script_state, detail);
}

AtomicString PerformanceMeasure::entryType() const {
  return performance_entry_names::kMeasure;
}

PerformanceEntryType PerformanceMeasure::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kMeasure;
}

}  // namespace blink
