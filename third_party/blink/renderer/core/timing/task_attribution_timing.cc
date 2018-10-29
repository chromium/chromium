// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/task_attribution_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"

namespace blink {

TaskAttributionTiming::TaskAttributionTiming(const AtomicString& name,
                                             const String& container_type,
                                             const String& container_src,
                                             const String& container_id,
                                             const String& container_name,
                                             double start_time,
                                             double finish_time,
                                             const String& script_url)
    : PerformanceEntry(name, start_time, finish_time),
      container_type_(container_type),
      container_src_(container_src),
      container_id_(container_id),
      container_name_(container_name),
      script_url_(script_url) {}

TaskAttributionTiming::~TaskAttributionTiming() = default;

AtomicString TaskAttributionTiming::entryType() const {
  return performance_entry_names::kTaskattribution;
}

PerformanceEntryType TaskAttributionTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kTaskAttribution;
}

String TaskAttributionTiming::scriptURL() const {
  return script_url_;
}

String TaskAttributionTiming::containerType() const {
  return container_type_;
}

String TaskAttributionTiming::containerSrc() const {
  return container_src_;
}

String TaskAttributionTiming::containerId() const {
  return container_id_;
}

String TaskAttributionTiming::containerName() const {
  return container_name_;
}

void TaskAttributionTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddString("containerType", containerType());
  builder.AddString("containerSrc", containerSrc());
  builder.AddString("containerId", containerId());
  builder.AddString("containerName", containerName());
}

void TaskAttributionTiming::Trace(blink::Visitor* visitor) {
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
