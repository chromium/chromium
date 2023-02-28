// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_long_animation_frame_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/task_attribution_timing.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
PerformanceLongAnimationFrameTiming::PerformanceLongAnimationFrameTiming(
    AnimationFrameTimingInfo* info,
    base::TimeTicks time_origin,
    bool cross_origin_isolated_capability,
    DOMWindow* source)
    : PerformanceEntry(
          info->Duration().InMilliseconds(),
          "long-animation-frame",
          DOMWindowPerformance::performance(*source->ToLocalDOMWindow())
              ->MonotonicTimeToDOMHighResTimeStamp(info->FrameStartTime()),
          source) {
  info_ = info;
  time_origin_ = time_origin;
  cross_origin_isolated_capability_ = cross_origin_isolated_capability;
}

PerformanceLongAnimationFrameTiming::~PerformanceLongAnimationFrameTiming() =
    default;

const AtomicString& PerformanceLongAnimationFrameTiming::entryType() const {
  return performance_entry_names::kLongAnimationFrame;
}

DOMHighResTimeStamp PerformanceLongAnimationFrameTiming::renderStart() const {
  return ToMonotonicTime(info_->RenderStartTime());
}

DOMHighResTimeStamp PerformanceLongAnimationFrameTiming::ToMonotonicTime(
    base::TimeTicks time) const {
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, time, /*allow_negative_value=*/false,
      cross_origin_isolated_capability_);
}

DOMHighResTimeStamp PerformanceLongAnimationFrameTiming::styleAndLayoutStart()
    const {
  return ToMonotonicTime(info_->StyleAndLayoutStartTime());
}

PerformanceEntryType PerformanceLongAnimationFrameTiming::EntryTypeEnum()
    const {
  return PerformanceEntry::EntryType::kLongAnimationFrame;
}

void PerformanceLongAnimationFrameTiming::BuildJSONValue(
    V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddNumber("renderStart", renderStart());
  builder.AddNumber("styleAndLayoutStart", styleAndLayoutStart());
}

void PerformanceLongAnimationFrameTiming::Trace(Visitor* visitor) const {
  PerformanceEntry::Trace(visitor);
  visitor->Trace(info_);
}

}  // namespace blink
