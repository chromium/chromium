// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/timing/performance_mark_conditional.h"

#include <optional>

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"

namespace blink {

PerformanceMarkConditional::PerformanceMarkConditional(
    const AtomicString& name,
    base::TimeTicks start_time,
    DOMWindow* source,
    uint32_t navigation_id)
    : PerformanceEntry(
          /*duration=*/0.0,
          name,
          DOMWindowPerformance::performance(*source->ToLocalDOMWindow())
              ->MonotonicTimeToDOMHighResTimeStamp(start_time),
          source,
          navigation_id) {}

const AtomicString& PerformanceMarkConditional::entryType() const {
  return performance_entry_names::kMarkConditional;
}

PerformanceEntryType PerformanceMarkConditional::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kMarkConditional;
}

}  // namespace blink
