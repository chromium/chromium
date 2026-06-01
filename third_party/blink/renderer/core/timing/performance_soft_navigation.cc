// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_soft_navigation.h"

#include "base/check_deref.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/interaction_contentful_paint.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"

namespace blink {

PerformanceSoftNavigation::PerformanceSoftNavigation(
    double start_time,
    const DOMPaintTimingInfo& paint_timing_info,
    SoftNavigationContext* context)
    : PerformanceEntry(
          /*duration=*/paint_timing_info.presentation_time - start_time,
          AtomicString(CHECK_DEREF(context).AttributionUrl()),
          start_time,
          CHECK_DEREF(context).DomWindow(),
          CHECK_DEREF(context).NavigationId()),
      navigation_type_(CHECK_DEREF(context).NavigationType()),
      interaction_id_(CHECK_DEREF(context).InteractionId()),
      context_(context) {
  SetPaintTimingInfo(paint_timing_info);
}

PerformanceSoftNavigation::~PerformanceSoftNavigation() = default;

const AtomicString& PerformanceSoftNavigation::entryType() const {
  return performance_entry_names::kSoftNavigation;
}

PerformanceEntryType PerformanceSoftNavigation::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kSoftNavigation;
}

InteractionContentfulPaint*
PerformanceSoftNavigation::getLargestInteractionContentfulPaint() const {
  CHECK(context_);
  return context_->LargestIcpEntry();
}

void PerformanceSoftNavigation::Trace(Visitor* visitor) const {
  visitor->Trace(context_);
  PerformanceEntry::Trace(visitor);
}

void PerformanceSoftNavigation::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddString("navigationType", navigationType().AsStringView());
  builder.AddNumber("interactionId", interaction_id_);
}

}  // namespace blink
