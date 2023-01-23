// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/visibility_state_entry.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"

namespace blink {

VisibilityStateEntry::VisibilityStateEntry(AtomicString name,
                                           double start_time,
                                           DOMWindow* source)
    : PerformanceEntry(name, start_time, start_time, source) {}

VisibilityStateEntry::~VisibilityStateEntry() = default;

const AtomicString& VisibilityStateEntry::entryType() const {
  return performance_entry_names::kVisibilityState;
}

PerformanceEntryType VisibilityStateEntry::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kVisibilityState;
}

}  // namespace blink
