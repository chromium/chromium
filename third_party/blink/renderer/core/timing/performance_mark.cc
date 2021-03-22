// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/timing/performance_mark.h"

#include "third_party/blink/public/mojom/timing/performance_mark_or_measure.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_performance_mark_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"

namespace blink {

PerformanceMark::PerformanceMark(
    const AtomicString& name,
    double start_time,
    scoped_refptr<SerializedScriptValue> serialized_detail,
    ExceptionState& exception_state)
    : PerformanceEntry(name, start_time, start_time),
      serialized_detail_(std::move(serialized_detail)) {}

// static
PerformanceMark* PerformanceMark::Create(ScriptState* script_state,
                                         const AtomicString& mark_name,
                                         PerformanceMarkOptions* mark_options,
                                         ExceptionState& exception_state) {
  Performance* performance = nullptr;
  bool is_worker_global_scope = false;
  if (LocalDOMWindow* window = LocalDOMWindow::From(script_state)) {
    performance = DOMWindowPerformance::performance(*window);
  } else if (auto* scope = DynamicTo<WorkerGlobalScope>(
                 ExecutionContext::From(script_state))) {
    performance = WorkerGlobalScopePerformance::performance(*scope);
    is_worker_global_scope = true;
  }
  DCHECK(performance);

  DOMHighResTimeStamp start = 0.0;
  ScriptValue detail = ScriptValue::CreateNull(script_state->GetIsolate());
  if (mark_options) {
    if (mark_options->hasStartTime()) {
      start = mark_options->startTime();
      if (start < 0.0) {
        exception_state.ThrowTypeError("'" + mark_name +
                                       "' cannot have a negative start time.");
        return nullptr;
      }
    } else {
      start = performance->now();
    }

    if (mark_options->hasDetail())
      detail = mark_options->detail();
  } else {
    start = performance->now();
  }

  if (!is_worker_global_scope &&
      PerformanceTiming::GetAttributeMapping().Contains(mark_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "'" + mark_name +
            "' is part of the PerformanceTiming interface, and "
            "cannot be used as a mark name.");
    return nullptr;
  }

  scoped_refptr<SerializedScriptValue> serialized_detail =
      SerializedScriptValue::Serialize(
          script_state->GetIsolate(), detail.V8Value(),
          SerializedScriptValue::SerializeOptions(), exception_state);
  if (exception_state.HadException())
    return nullptr;

  return MakeGarbageCollected<PerformanceMark>(
      mark_name, start, std::move(serialized_detail), exception_state);
}

AtomicString PerformanceMark::entryType() const {
  return performance_entry_names::kMark;
}

PerformanceEntryType PerformanceMark::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kMark;
}

mojom::blink::PerformanceMarkOrMeasurePtr
PerformanceMark::ToMojoPerformanceMarkOrMeasure() {
  auto mojo_performance_mark_or_measure =
      PerformanceEntry::ToMojoPerformanceMarkOrMeasure();
  mojo_performance_mark_or_measure->detail = serialized_detail_->GetWireData();
  return mojo_performance_mark_or_measure;
}

ScriptValue PerformanceMark::detail(ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (!serialized_detail_)
    return ScriptValue(isolate, v8::Null(isolate));
  auto result = deserialized_detail_map_.insert(
      script_state, TraceWrapperV8Reference<v8::Value>());
  TraceWrapperV8Reference<v8::Value>& relevant_data =
      result.stored_value->value;
  if (!result.is_new_entry)
    return ScriptValue(isolate, relevant_data.NewLocal(isolate));
  v8::Local<v8::Value> value = serialized_detail_->Deserialize(isolate);
  relevant_data.Set(isolate, value);
  return ScriptValue(isolate, value);
}

void PerformanceMark::Trace(Visitor* visitor) const {
  visitor->Trace(deserialized_detail_map_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
