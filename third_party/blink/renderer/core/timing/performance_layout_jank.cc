// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_layout_jank.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// static
PerformanceLayoutJank* PerformanceLayoutJank::Create(double fraction) {
  return new PerformanceLayoutJank(fraction);
}

PerformanceLayoutJank::PerformanceLayoutJank(double fraction)
    : PerformanceEntry(g_empty_atom, 0.0, 0.0), fraction_(fraction) {}

PerformanceLayoutJank::~PerformanceLayoutJank() = default;

AtomicString PerformanceLayoutJank::entryType() const {
  return performance_entry_names::kLayoutJank;
}

PerformanceEntryType PerformanceLayoutJank::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kLayoutJank;
}

void PerformanceLayoutJank::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.Add("fraction", fraction_);
}

void PerformanceLayoutJank::Trace(blink::Visitor* visitor) {
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
