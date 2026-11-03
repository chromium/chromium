// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/interaction_contentful_paint.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"
#include "third_party/blink/renderer/core/timing/performance_timeline_entry_id_generator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

InteractionContentfulPaint::InteractionContentfulPaint(
    double start_time,
    LargestContentfulPaint* largest_contentful_paint,
    DOMWindow* source,
    uint64_t interaction_id)
    : PerformanceEntry(
          /*duration=*/0.0,
          g_empty_atom,
          start_time,
          source,
          /*navigation_id=*/
          PerformanceTimelineEntryIdInfo::kNone.web_exposed_id),
      largest_contentful_paint_(largest_contentful_paint),
      interaction_id_(interaction_id) {
  CHECK(largest_contentful_paint_);
}

InteractionContentfulPaint::~InteractionContentfulPaint() = default;

const AtomicString& InteractionContentfulPaint::entryType() const {
  return performance_entry_names::kInteractionContentfulPaint;
}

PerformanceEntryType InteractionContentfulPaint::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kInteractionContentfulPaint;
}

DOMHighResTimeStamp InteractionContentfulPaint::duration() const {
  return GetPaintTimingInfo().presentation_time - startTime();
}

bool InteractionContentfulPaint::IsReadyForReporting() const {
  return navigationId() !=
             PerformanceTimelineEntryIdInfo::kNone.web_exposed_id &&
         HasPaintTimingInfo() &&
         largest_contentful_paint_->IsReadyForReporting();
}

void InteractionContentfulPaint::FinalizeEntry(
    const DOMPaintTimingInfo& paint_timing_info,
    uint64_t navigation_id) {
  CHECK(!IsReadyForReporting());
  PerformanceEntry::FinalizeEntry(paint_timing_info, navigation_id);
  largest_contentful_paint_->FinalizeEntry(paint_timing_info, navigation_id);
  CHECK(IsReadyForReporting());
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
