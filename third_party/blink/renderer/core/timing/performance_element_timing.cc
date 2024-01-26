// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_element_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/performance.h"

namespace blink {

// static
PerformanceElementTiming* PerformanceElementTiming::Create(
    const AtomicString& name,
    const String& url,
    const gfx::RectF& intersection_rect,
    DOMHighResTimeStamp render_time,
    DOMHighResTimeStamp load_time,
    const AtomicString& identifier,
    int naturalWidth,
    int naturalHeight,
    const AtomicString& id,
    Element* element,
    DOMWindow* source) {
  // It is possible to 'paint' images which have naturalWidth or naturalHeight
  // equal to 0.
  DCHECK_GE(naturalWidth, 0);
  DCHECK_GE(naturalHeight, 0);
  DCHECK(element);
  double start_time = render_time != 0.0 ? render_time : load_time;
  return MakeGarbageCollected<PerformanceElementTiming>(
      name, start_time, url, intersection_rect, render_time, load_time,
      identifier, naturalWidth, naturalHeight, id, element, source);
}

PerformanceElementTiming::PerformanceElementTiming(
    const AtomicString& name,
    DOMHighResTimeStamp start_time,
    const String& url,
    const gfx::RectF& intersection_rect,
    DOMHighResTimeStamp render_time,
    DOMHighResTimeStamp load_time,
    const AtomicString& identifier,
    int naturalWidth,
    int naturalHeight,
    const AtomicString& id,
    Element* element,
    DOMWindow* source)
    : PerformanceEntry(name, start_time, start_time, source),
      element_(element),
      intersection_rect_(DOMRectReadOnly::FromRectF(intersection_rect)),
      render_time_(render_time),
      load_time_(load_time),
      identifier_(identifier),
      naturalWidth_(naturalWidth),
      naturalHeight_(naturalHeight),
      id_(id),
      url_(url) {}

PerformanceElementTiming::~PerformanceElementTiming() = default;

const AtomicString& PerformanceElementTiming::entryType() const {
  return performance_entry_names::kElement;
}

PerformanceEntryType PerformanceElementTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kElement;
}

Element* PerformanceElementTiming::element() const {
  return Performance::CanExposeNode(element_) ? element_ : nullptr;
}

std::unique_ptr<TracedValue> PerformanceElementTiming::ToTracedValue() const {
  auto traced_value = std::make_unique<TracedValue>();
  traced_value->SetString("elementType", name());
  traced_value->SetInteger("loadTime", load_time_);
  traced_value->SetInteger("renderTime", render_time_);
  traced_value->SetDouble("rectLeft", intersection_rect_->left());
  traced_value->SetDouble("rectTop", intersection_rect_->top());
  traced_value->SetDouble("rectWidth", intersection_rect_->width());
  traced_value->SetDouble("rectHeight", intersection_rect_->height());
  traced_value->SetString("identifier", identifier_);
  traced_value->SetInteger("naturalWidth", naturalWidth_);
  traced_value->SetInteger("naturalHeight", naturalHeight_);
  traced_value->SetString("elementId", id_);
  traced_value->SetString("url", url_);
  return traced_value;
}

void PerformanceElementTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddNumber("renderTime", render_time_);
  builder.AddNumber("loadTime", load_time_);
  builder.Add("intersectionRect", intersection_rect_.Get());
  builder.AddString("identifier", identifier_);
  builder.AddNumber("naturalWidth", naturalWidth_);
  builder.AddNumber("naturalHeight", naturalHeight_);
  builder.AddString("id", id_);
  builder.AddString("url", url_);
}

void PerformanceElementTiming::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(intersection_rect_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
