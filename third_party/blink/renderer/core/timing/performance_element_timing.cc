// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_element_timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"

namespace blink {

// static
PerformanceElementTiming* PerformanceElementTiming::Create(
    const AtomicString& name,
    const String& url,
    const FloatRect& intersection_rect,
    DOMHighResTimeStamp render_time,
    DOMHighResTimeStamp load_time,
    const AtomicString& identifier,
    int naturalWidth,
    int naturalHeight,
    const AtomicString& id,
    Element* element) {
  // It is possible to 'paint' images which have naturalWidth or naturalHeight
  // equal to 0.
  DCHECK_GE(naturalWidth, 0);
  DCHECK_GE(naturalHeight, 0);
  DCHECK(element);
  double start_time = render_time != 0.0 ? render_time : load_time;
  return MakeGarbageCollected<PerformanceElementTiming>(
      name, start_time, url, intersection_rect, render_time, load_time,
      identifier, naturalWidth, naturalHeight, id, element);
}

PerformanceElementTiming::PerformanceElementTiming(
    const AtomicString& name,
    DOMHighResTimeStamp start_time,
    const String& url,
    const FloatRect& intersection_rect,
    DOMHighResTimeStamp render_time,
    DOMHighResTimeStamp load_time,
    const AtomicString& identifier,
    int naturalWidth,
    int naturalHeight,
    const AtomicString& id,
    Element* element)
    : PerformanceEntry(name, start_time, start_time),
      element_(element),
      intersection_rect_(DOMRectReadOnly::FromFloatRect(intersection_rect)),
      render_time_(render_time),
      load_time_(load_time),
      identifier_(identifier),
      naturalWidth_(naturalWidth),
      naturalHeight_(naturalHeight),
      id_(id),
      url_(url) {}

PerformanceElementTiming::~PerformanceElementTiming() = default;

AtomicString PerformanceElementTiming::entryType() const {
  return performance_entry_names::kElement;
}

PerformanceEntryType PerformanceElementTiming::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kElement;
}

Element* PerformanceElementTiming::element() const {
  if (!element_ || !element_->isConnected() || element_->IsInShadowTree())
    return nullptr;

  // Do not expose |element_| when the document is not 'fully active'.
  const Document& document = element_->GetDocument();
  if (!document.IsActive() || !document.GetFrame())
    return nullptr;

  return element_;
}

void PerformanceElementTiming::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.Add("renderTime", render_time_);
  builder.Add("loadTime", load_time_);
  builder.Add("intersectionRect", intersection_rect_);
  builder.Add("identifier", identifier_);
  builder.Add("naturalWidth", naturalWidth_);
  builder.Add("naturalHeight", naturalHeight_);
  builder.Add("id", id_);
  builder.Add("element", element());
  builder.Add("url", url_);
}

void PerformanceElementTiming::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_);
  visitor->Trace(intersection_rect_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
