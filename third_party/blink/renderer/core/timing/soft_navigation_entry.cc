// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_entry.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"

namespace blink {

SoftNavigationEntry::SoftNavigationEntry(
    AtomicString name,
    double start_time,
    const DOMPaintTimingInfo& paint_timing_info,
    DOMWindow* source,
    uint32_t navigation_id)
    : PerformanceEntry(/*duration=*/0.0,
                       name,
                       start_time,
                       source,
                       navigation_id) {
  SetPaintTimingInfo(paint_timing_info);
}

SoftNavigationEntry::~SoftNavigationEntry() = default;

const AtomicString& SoftNavigationEntry::entryType() const {
  return performance_entry_names::kSoftNavigation;
}

PerformanceEntryType SoftNavigationEntry::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kSoftNavigation;
}

}  // namespace blink
