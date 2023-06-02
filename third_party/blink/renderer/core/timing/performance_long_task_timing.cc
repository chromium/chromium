// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_long_task_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/task_attribution_timing.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

PerformanceLongTaskTiming::PerformanceLongTaskTiming(
    double start_time,
    int duration,
    const AtomicString& name,
    const AtomicString& culprit_type,
    const AtomicString& culprit_src,
    const AtomicString& culprit_id,
    const AtomicString& culprit_name,
    DOMWindow* source)
    : PerformanceEntry(duration, name, start_time, source) {
  auto* attribution_entry = MakeGarbageCollected<TaskAttributionTiming>(
      AtomicString("unknown"), culprit_type, culprit_src, culprit_id,
      culprit_name, source);
  attribution_.push_back(*attribution_entry);
}

PerformanceLongTaskTiming::~PerformanceLongTaskTiming() = default;

const AtomicString& PerformanceLongTaskTiming::entryType() const {
  return performance_entry_names::kLongtask;
}

PerformanceEntryType PerformanceLongTaskTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kLongTask;
}

TaskAttributionVector PerformanceLongTaskTiming::attribution() const {
  return attribution_;
}

void PerformanceLongTaskTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  ScriptState* script_state = builder.GetScriptState();
  builder.Add("attribution", FreezeV8Object(ToV8(attribution_, script_state),
                                            script_state->GetIsolate()));
}

void PerformanceLongTaskTiming::Trace(Visitor* visitor) const {
  visitor->Trace(attribution_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
