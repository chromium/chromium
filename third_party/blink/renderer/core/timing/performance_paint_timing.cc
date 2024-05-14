// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_paint_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"

namespace blink {

namespace {

AtomicString FromPaintTypeToString(PerformancePaintTiming::PaintType type) {
  DCHECK(IsMainThread());
  switch (type) {
    case PerformancePaintTiming::PaintType::kFirstPaint: {
      DEFINE_STATIC_LOCAL(const AtomicString, kFirstPaint, ("first-paint"));
      return kFirstPaint;
    }
    case PerformancePaintTiming::PaintType::kFirstContentfulPaint: {
      DEFINE_STATIC_LOCAL(const AtomicString, kFirstContentfulPaint,
                          ("first-contentful-paint"));
      return kFirstContentfulPaint;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return g_empty_atom;
}

}  // namespace

PerformancePaintTiming::PerformancePaintTiming(
    PaintType type,
    double start_time,
    DOMWindow* source,
    bool is_triggered_by_soft_navigation)
    : PerformanceEntry(FromPaintTypeToString(type),
                       start_time,
                       start_time,
                       source,
                       is_triggered_by_soft_navigation) {}

PerformancePaintTiming::~PerformancePaintTiming() = default;

const AtomicString& PerformancePaintTiming::entryType() const {
  return performance_entry_names::kPaint;
}

PerformanceEntryType PerformancePaintTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kPaint;
}

}  // namespace blink
