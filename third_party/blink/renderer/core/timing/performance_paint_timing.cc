// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_paint_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"

namespace blink {

namespace {

const AtomicString& FromPaintTypeToString(
    PerformancePaintTiming::PaintType type) {
  switch (type) {
    case PerformancePaintTiming::PaintType::kFirstPaint:
      return performance_entry_names::kFirstPaint;
    case PerformancePaintTiming::PaintType::kFirstContentfulPaint:
      return performance_entry_names::kFirstContentfulPaint;
  }
  NOTREACHED();
}

}  // namespace

PerformancePaintTiming::PerformancePaintTiming(
    PaintType type,
    const DOMPaintTimingInfo& paint_timing_info,
    DOMWindow* source,
    uint64_t navigation_id)
    : PerformanceEntry(
          /*duration=*/0.0,
          FromPaintTypeToString(type),
          // https://w3c.github.io/paint-timing/#report-paint-timing
          // Set newEntry’s startTime attribute to the default paint timestamp
          // given paintTimingInfo.
          paint_timing_info.presentation_time,
          source,
          navigation_id) {
  SetPaintTimingInfo(paint_timing_info);
}

PerformancePaintTiming::~PerformancePaintTiming() = default;

const AtomicString& PerformancePaintTiming::entryType() const {
  return performance_entry_names::kPaint;
}

PerformanceEntryType PerformancePaintTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kPaint;
}

}  // namespace blink
