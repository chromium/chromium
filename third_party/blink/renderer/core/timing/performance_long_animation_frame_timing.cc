// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_long_animation_frame_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_script_timing.h"
#include "third_party/blink/renderer/core/timing/task_attribution_timing.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

PerformanceLongAnimationFrameTiming*
PerformanceLongAnimationFrameTiming::Create(
    AnimationFrameTimingInfo* info,
    base::TimeTicks time_origin,
    bool cross_origin_isolated_capability,
    DOMWindow* source,
    const std::optional<DOMPaintTimingInfo>& paint_timing_info,
    uint32_t navigation_id) {
  Performance* performance =
      DOMWindowPerformance::performance(*source->ToLocalDOMWindow());
  DOMHighResTimeStamp startTime =
      performance->MonotonicTimeToDOMHighResTimeStamp(info->FrameStartTime());
  double duration = paint_timing_info
                        ? (paint_timing_info->paint_time - startTime)
                        : info->Duration().InMillisecondsF();
  PerformanceLongAnimationFrameTiming* entry =
      MakeGarbageCollected<PerformanceLongAnimationFrameTiming>(
          duration, startTime, info, time_origin,
          cross_origin_isolated_capability, source, navigation_id);
  if (paint_timing_info.has_value()) {
    entry->SetPaintTimingInfo(*paint_timing_info);
  }
  return entry;
}

PerformanceLongAnimationFrameTiming::PerformanceLongAnimationFrameTiming(
    double duration,
    DOMHighResTimeStamp startTime,
    AnimationFrameTimingInfo* info,
    base::TimeTicks time_origin,
    bool cross_origin_isolated_capability,
    DOMWindow* source,
    uint32_t navigation_id)
    : PerformanceEntry(duration,
                       AtomicString("long-animation-frame"),
                       startTime,
                       source,
                       navigation_id),
      render_start_(Performance::MonotonicTimeToDOMHighResTimeStamp(
          time_origin,
          info->RenderStartTime(),
          /*allow_negative_value=*/false,
          cross_origin_isolated_capability)),
      style_and_layout_start_(Performance::MonotonicTimeToDOMHighResTimeStamp(
          time_origin,
          info->StyleAndLayoutStartTime(),
          /*allow_negative_value=*/false,
          cross_origin_isolated_capability)),
      first_ui_event_timestamp_(Performance::MonotonicTimeToDOMHighResTimeStamp(
          time_origin,
          info->FirstUIEventTime(),
          /*allow_negative_value=*/false,
          cross_origin_isolated_capability)),
      blocking_duration_(info->TotalBlockingDuration().InMillisecondsF()) {
  CHECK(source->ToLocalDOMWindow());
  const SecurityOrigin* security_origin =
      source->ToLocalDOMWindow()->GetSecurityOrigin();
  CHECK(security_origin);

  for (ScriptTimingInfo* script : info->Scripts()) {
    if (security_origin->CanAccess(script->GetSecurityOrigin())) {
      scripts_.push_back(MakeGarbageCollected<PerformanceScriptTiming>(
          script, time_origin, cross_origin_isolated_capability, source,
          navigation_id));
    }
  }
}

PerformanceLongAnimationFrameTiming::~PerformanceLongAnimationFrameTiming() =
    default;

const AtomicString& PerformanceLongAnimationFrameTiming::entryType() const {
  return performance_entry_names::kLongAnimationFrame;
}

PerformanceEntryType PerformanceLongAnimationFrameTiming::EntryTypeEnum()
    const {
  return PerformanceEntry::EntryType::kLongAnimationFrame;
}

void PerformanceLongAnimationFrameTiming::BuildJSONValue(
    V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddNumber("renderStart", render_start_);
  builder.AddNumber("styleAndLayoutStart", style_and_layout_start_);
  builder.AddNumber("firstUIEventTimestamp", first_ui_event_timestamp_);
  builder.AddNumber("blockingDuration", blocking_duration_);
  builder.AddV8Value("scripts",
                     ToV8Traits<IDLArray<PerformanceScriptTiming>>::ToV8(
                         builder.GetScriptState(), scripts_));
}

void PerformanceLongAnimationFrameTiming::Trace(Visitor* visitor) const {
  PerformanceEntry::Trace(visitor);
  visitor->Trace(scripts_);
}

}  // namespace blink
