// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_measure_conditional.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"

namespace blink {

PerformanceMeasureConditional* PerformanceMeasureConditional::Create(
    const AtomicString& name,
    base::TimeTicks start_time,
    base::TimeTicks end_time,
    LocalDOMWindow* window,
    DOMWindow* source,
    uint64_t navigation_id) {
  Performance* performance =
      DOMWindowPerformance::performance(*source->ToLocalDOMWindow());
  DOMHighResTimeStamp start =
      performance->MonotonicTimeToDOMHighResTimeStamp(start_time);
  DOMHighResTimeStamp end =
      performance->MonotonicTimeToDOMHighResTimeStamp(end_time);
  return MakeGarbageCollected<PerformanceMeasureConditional>(
      name, start, end - start, window, source, navigation_id);
}

PerformanceMeasureConditional::PerformanceMeasureConditional(
    const AtomicString& name,
    double start_time,
    double duration,
    LocalDOMWindow* window,
    DOMWindow* source,
    uint64_t navigation_id)
    : PerformanceEntry(duration, name, start_time, source, navigation_id),
      window_(window) {}

DOMWindow* PerformanceMeasureConditional::window() const {
  return window_ && window_->GetFrame() ? window_ : nullptr;
}

void PerformanceMeasureConditional::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  PerformanceEntry::Trace(visitor);
}

const AtomicString& PerformanceMeasureConditional::entryType() const {
  return performance_entry_names::kMeasureConditional;
}

PerformanceEntryType PerformanceMeasureConditional::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kMeasureConditional;
}

}  // namespace blink
