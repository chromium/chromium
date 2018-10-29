// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_long_task_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/sub_task_attribution.h"
#include "third_party/blink/renderer/core/timing/task_attribution_timing.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// static
PerformanceLongTaskTiming* PerformanceLongTaskTiming::Create(
    double start_time,
    double end_time,
    const AtomicString& name,
    const String& frame_src,
    const String& frame_id,
    const String& frame_name,
    const SubTaskAttribution::EntriesVector& sub_task_attributions) {
  return new PerformanceLongTaskTiming(start_time, end_time, name, frame_src,
                                       frame_id, frame_name,
                                       sub_task_attributions);
}

PerformanceLongTaskTiming::PerformanceLongTaskTiming(
    double start_time,
    double end_time,
    const AtomicString& name,
    const String& culprit_frame_src,
    const String& culprit_frame_id,
    const String& culprit_frame_name,
    const SubTaskAttribution::EntriesVector& sub_task_attributions)
    : PerformanceEntry(name, start_time, end_time) {
  // Only one possible container type exists currently: "iframe".
  if (RuntimeEnabledFeatures::LongTaskV2Enabled()) {
    for (auto&& it : sub_task_attributions) {
      TaskAttributionTiming* attribution_entry = TaskAttributionTiming::Create(
          it->subTaskName(), "iframe", culprit_frame_src, culprit_frame_id,
          culprit_frame_name, it->highResStartTime(),
          it->highResStartTime() + it->highResDuration(), it->scriptURL());
      attribution_.push_back(*attribution_entry);
    }
  } else {
    // Only one possible task type exists currently: "script".
    TaskAttributionTiming* attribution_entry =
        TaskAttributionTiming::Create("script", "iframe", culprit_frame_src,
                                      culprit_frame_id, culprit_frame_name);
    attribution_.push_back(*attribution_entry);
  }
}

PerformanceLongTaskTiming::~PerformanceLongTaskTiming() = default;

AtomicString PerformanceLongTaskTiming::entryType() const {
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
  Vector<ScriptValue> attribution;
  for (unsigned i = 0; i < attribution_.size(); i++) {
    attribution.push_back(
        attribution_[i]->toJSONForBinding(builder.GetScriptState()));
  }
  builder.Add("attribution", attribution);
}

void PerformanceLongTaskTiming::Trace(blink::Visitor* visitor) {
  visitor->Trace(attribution_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
