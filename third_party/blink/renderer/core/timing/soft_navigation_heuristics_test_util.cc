// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics_test_util.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyboard_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mouse_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/core/timing/performance_timeline_entry_id_generator.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

PerformanceEventTiming* CreatePerformanceEventTimingForTest(
    const AtomicString& event_type,
    base::TimeTicks start_time,
    EventTarget* target,
    DOMWindow* source) {
  PerformanceEventTiming::EventTimingReportingInfo reporting_info;
  reporting_info.creation_time = start_time;
  reporting_info.processing_start_time = start_time + base::Milliseconds(1);
  reporting_info.processing_end_time = start_time + base::Milliseconds(2);
  reporting_info.frame_index = 1;

  return PerformanceEventTiming::Create(
      event_type, reporting_info, /*cancelable=*/true, target, source,
      /*navigation_id=*/1, PerformanceTimelineEntryIdInfo(1, 1));
}

TextRecord* CreateTextRecordForTest(Node* node,
                                    int width,
                                    int height,
                                    SoftNavigationContext* context) {
  return MakeGarbageCollected<TextRecord>(
      node, width * height, gfx::RectF(width, height), gfx::Rect(width, height),
      gfx::RectF(width, height),
      /* is_needed_for_timing=*/false, context);
}

}  // namespace blink
