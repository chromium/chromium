// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/layout_shift.h"

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
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
                                 DOMWindow* source) {
  return MakeGarbageCollected<LayoutShift>(start_time, value, input_detected,
                                           input_timestamp, sources, source);
}

LayoutShift::LayoutShift(double start_time,
                         double value,
                         bool input_detected,
                         double input_timestamp,
                         AttributionList sources,
                         DOMWindow* source)
    : PerformanceEntry(g_empty_atom, start_time, start_time, source),
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
  builder.AddNumber("value", value_);
  builder.AddBoolean("hadRecentInput", had_recent_input_);
  builder.AddNumber("lastInputTime", most_recent_input_timestamp_);
  builder.AddV8Value("sources",
                     ToV8Traits<IDLArray<LayoutShiftAttribution>>::ToV8(
                         builder.GetScriptState(), sources_));
}

void LayoutShift::Trace(Visitor* visitor) const {
  PerformanceEntry::Trace(visitor);
  visitor->Trace(sources_);
}

}  // namespace blink
