// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_container_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/performance.h"

namespace blink {

// static
PerformanceContainerTiming* PerformanceContainerTiming::Create(
    const gfx::Rect& intersection_rect,
    double size,
    Element* root_element,
    const AtomicString& identifier,
    Element* last_painted_element,
    DOMWindow* source) {
  return MakeGarbageCollected<PerformanceContainerTiming>(
      intersection_rect, size, root_element, identifier, last_painted_element,
      source);
}

PerformanceContainerTiming::PerformanceContainerTiming(
    const gfx::Rect& intersection_rect,
    double size,
    Element* root_element,
    const AtomicString& identifier,
    Element* last_painted_element,
    DOMWindow* source)
    : PerformanceEntry(
          /*duration=*/0.0,
          g_empty_atom,
          /*start_time=*/0.0,
          source,
          /*navigation_id=*/
          PerformanceTimelineEntryIdInfo::kNone.web_exposed_id),
      intersection_rect_(DOMRectReadOnly::FromRect(intersection_rect)),
      size_(size),
      root_element_(root_element),
      identifier_(identifier),
      last_painted_element_(last_painted_element) {}

PerformanceContainerTiming::~PerformanceContainerTiming() = default;

const AtomicString& PerformanceContainerTiming::entryType() const {
  return performance_entry_names::kContainer;
}

PerformanceEntryType PerformanceContainerTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kContainer;
}

DOMHighResTimeStamp PerformanceContainerTiming::startTime() const {
  return GetPaintTimingInfo().presentation_time;
}

bool PerformanceContainerTiming::IsReadyForReporting() const {
  return navigationId() !=
             PerformanceTimelineEntryIdInfo::kNone.web_exposed_id &&
         HasPaintTimingInfo() && first_render_time_ != 0.0;
}

void PerformanceContainerTiming::FinalizeEntry(
    const DOMPaintTimingInfo& paint_timing_info,
    uint64_t navigation_id) {
  CHECK(!IsReadyForReporting());
  PerformanceEntry::FinalizeEntry(paint_timing_info, navigation_id);
  CHECK(IsReadyForReporting());
}

void PerformanceContainerTiming::FinalizeEntry(
    const DOMPaintTimingInfo& paint_timing_info,
    uint64_t navigation_id,
    DOMHighResTimeStamp first_render_time) {
  first_render_time_ = first_render_time;
  FinalizeEntry(paint_timing_info, navigation_id);
}

Element* PerformanceContainerTiming::rootElement() const {
  return Performance::CanExposeNode(root_element_, source()) ? root_element_
                                                             : nullptr;
}

Element* PerformanceContainerTiming::lastPaintedElement() const {
  return Performance::CanExposeNode(last_painted_element_, source())
             ? last_painted_element_
             : nullptr;
}

std::unique_ptr<TracedValue> PerformanceContainerTiming::ToTracedValue() const {
  auto traced_value = std::make_unique<TracedValue>();
  traced_value->SetString("elementType", name());
  traced_value->SetInteger("startTime", startTime());
  traced_value->SetInteger("duration", duration());
  traced_value->SetInteger("firstRenderTime", firstRenderTime());
  traced_value->SetDouble("rectLeft", intersection_rect_->left());
  traced_value->SetDouble("rectTop", intersection_rect_->top());
  traced_value->SetDouble("rectWidth", intersection_rect_->width());
  traced_value->SetDouble("rectHeight", intersection_rect_->height());
  traced_value->SetDouble("size", size_);
  traced_value->SetString("identifier", identifier_);
  return traced_value;
}

void PerformanceContainerTiming::BuildJSONValue(
    V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.Add("intersectionRect", intersection_rect_.Get());
  builder.AddNumber("size", size_);
  builder.AddString("identifier", identifier_);
  builder.AddNumber("firstRenderTime", first_render_time_);
}

void PerformanceContainerTiming::Trace(Visitor* visitor) const {
  visitor->Trace(intersection_rect_);
  visitor->Trace(root_element_);
  visitor->Trace(last_painted_element_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
