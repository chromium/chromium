// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_element_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"

namespace blink {

// static
PerformanceElementTiming* PerformanceElementTiming::Create(
    const AtomicString& name,
    const IntRect& intersection_rect,
    DOMHighResTimeStamp start_time) {
  return new PerformanceElementTiming(name, intersection_rect, start_time);
}

PerformanceElementTiming::PerformanceElementTiming(
    const AtomicString& name,
    const IntRect& intersection_rect,
    DOMHighResTimeStamp start_time)
    : PerformanceEntry(name, start_time, start_time),
      intersection_rect_(DOMRectReadOnly::FromIntRect(intersection_rect)) {}

PerformanceElementTiming::~PerformanceElementTiming() = default;

AtomicString PerformanceElementTiming::entryType() const {
  return performance_entry_names::kElement;
}

PerformanceEntryType PerformanceElementTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kElement;
}

void PerformanceElementTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.Add("intersectionRect", intersection_rect_);
}

void PerformanceElementTiming::Trace(blink::Visitor* visitor) {
  visitor->Trace(intersection_rect_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
