// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/interaction_contentful_paint.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

InteractionContentfulPaint::InteractionContentfulPaint(
    double start_time,
    DOMHighResTimeStamp render_time,
    LargestContentfulPaint* largest_contentful_paint,
    DOMWindow* source,
    uint32_t navigation_id,
    uint64_t interaction_id)
    : PerformanceEntry(/*duration=*/render_time - start_time,
                       g_empty_atom,
                       start_time,
                       source,
                       navigation_id),
      largest_contentful_paint_(largest_contentful_paint),
      interaction_id_(interaction_id) {}

InteractionContentfulPaint::~InteractionContentfulPaint() = default;

const AtomicString& InteractionContentfulPaint::entryType() const {
  return performance_entry_names::kInteractionContentfulPaint;
}

PerformanceEntryType InteractionContentfulPaint::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kInteractionContentfulPaint;
}

void InteractionContentfulPaint::BuildJSONValue(
    V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddNumber("interactionId", interaction_id_);
  builder.Add("largestContentfulPaint", largest_contentful_paint_.Get());
}

void InteractionContentfulPaint::Trace(Visitor* visitor) const {
  visitor->Trace(largest_contentful_paint_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
