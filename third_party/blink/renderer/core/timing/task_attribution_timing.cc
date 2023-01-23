// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/task_attribution_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"

namespace blink {

TaskAttributionTiming::TaskAttributionTiming(const AtomicString& name,
                                             const AtomicString& container_type,
                                             const AtomicString& container_src,
                                             const AtomicString& container_id,
                                             const AtomicString& container_name,
                                             DOMWindow* source)
    : PerformanceEntry(name, 0.0, 0.0, source),
      container_type_(container_type),
      container_src_(container_src),
      container_id_(container_id),
      container_name_(container_name) {}

TaskAttributionTiming::~TaskAttributionTiming() = default;

const AtomicString& TaskAttributionTiming::entryType() const {
  return performance_entry_names::kTaskattribution;
}

PerformanceEntryType TaskAttributionTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kTaskAttribution;
}

AtomicString TaskAttributionTiming::containerType() const {
  return container_type_;
}

AtomicString TaskAttributionTiming::containerSrc() const {
  return container_src_;
}

AtomicString TaskAttributionTiming::containerId() const {
  return container_id_;
}

AtomicString TaskAttributionTiming::containerName() const {
  return container_name_;
}

void TaskAttributionTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddString("containerType", containerType());
  builder.AddString("containerSrc", containerSrc());
  builder.AddString("containerId", containerId());
  builder.AddString("containerName", containerName());
}

void TaskAttributionTiming::Trace(Visitor* visitor) const {
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
