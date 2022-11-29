// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/layout_shift.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// static
LayoutShift* LayoutShift::Create(double start_time,
                                 double value,
                                 bool input_detected,
                                 double input_timestamp,
                                 AttributionList sources,
                                 uint32_t navigation_id) {
  return MakeGarbageCollected<LayoutShift>(start_time, value, input_detected,
                                           input_timestamp, sources,
                                           navigation_id);
}

LayoutShift::LayoutShift(double start_time,
                         double value,
                         bool input_detected,
                         double input_timestamp,
                         AttributionList sources,
                         uint32_t navigation_id)
    : PerformanceEntry(g_empty_atom, start_time, start_time, navigation_id),
      value_(value),
      had_recent_input_(input_detected),
      most_recent_input_timestamp_(input_timestamp),
      sources_(sources) {}

LayoutShift::~LayoutShift() = default;

const AtomicString& LayoutShift::entryType() const {
  return performance_entry_names::kLayoutShift;
}

PerformanceEntryType LayoutShift::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kLayoutShift;
}

void LayoutShift::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.Add("value", value_);
  builder.Add("hadRecentInput", had_recent_input_);
  builder.Add("lastInputTime", most_recent_input_timestamp_);

  ScriptState* script_state = builder.GetScriptState();
  builder.Add("sources", FreezeV8Object(ToV8(sources_, script_state),
                                        script_state->GetIsolate()));
}

void LayoutShift::Trace(Visitor* visitor) const {
  PerformanceEntry::Trace(visitor);
  visitor->Trace(sources_);
}

}  // namespace blink
