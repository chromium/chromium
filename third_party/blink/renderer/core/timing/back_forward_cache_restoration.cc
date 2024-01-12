// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/back_forward_cache_restoration.h"

#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"

namespace blink {
BackForwardCacheRestoration::BackForwardCacheRestoration(
    DOMHighResTimeStamp start_time,
    DOMHighResTimeStamp pageshow_event_start,
    DOMHighResTimeStamp pageshow_event_end,
    DOMWindow* source)
    : PerformanceEntry(g_empty_atom, start_time, pageshow_event_start, source),
      pageshow_event_start_(pageshow_event_start),
      pageshow_event_end_(pageshow_event_end) {}
BackForwardCacheRestoration::~BackForwardCacheRestoration() = default;
const AtomicString& BackForwardCacheRestoration::entryType() const {
  return performance_entry_names::kBackForwardCacheRestoration;
}
PerformanceEntryType BackForwardCacheRestoration::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kBackForwardCacheRestoration;
}
void BackForwardCacheRestoration::Trace(Visitor* visitor) const {
  PerformanceEntry::Trace(visitor);
}
void BackForwardCacheRestoration::BuildJSONValue(
    V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddNumber("pageshowEventStart", pageshow_event_start_);
  builder.AddNumber("pageshowEventEnd", pageshow_event_end_);
}
}  // namespace blink
