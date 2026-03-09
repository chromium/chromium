// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_entry.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/interaction_contentful_paint.h"

namespace blink {

SoftNavigationEntry::SoftNavigationEntry(
    AtomicString name,
    double start_time,
    const DOMPaintTimingInfo& paint_timing_info,
    DOMWindow* source,
    uint32_t navigation_id,
    V8NavigationType::Enum navigation_type,
    uint64_t interaction_id,
    InteractionContentfulPaint* largest_interaction_contentful_paint)
    : PerformanceEntry(
          /*duration=*/paint_timing_info.presentation_time - start_time,
          name,
          start_time,
          source,
          navigation_id),
      navigation_type_(navigation_type),
      interaction_id_(interaction_id),
      largest_interaction_contentful_paint_(
          largest_interaction_contentful_paint) {
  SetPaintTimingInfo(paint_timing_info);
}

SoftNavigationEntry::~SoftNavigationEntry() = default;

const AtomicString& SoftNavigationEntry::entryType() const {
  return performance_entry_names::kSoftNavigation;
}

PerformanceEntryType SoftNavigationEntry::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kSoftNavigation;
}

void SoftNavigationEntry::Trace(Visitor* visitor) const {
  visitor->Trace(largest_interaction_contentful_paint_);
  PerformanceEntry::Trace(visitor);
}

void SoftNavigationEntry::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddString("navigationType", navigationType().AsStringView());
  builder.AddNumber("interactionId", interaction_id_);
  builder.Add("largestInteractionContentfulPaint",
              largest_interaction_contentful_paint_.Get());
}

}  // namespace blink
