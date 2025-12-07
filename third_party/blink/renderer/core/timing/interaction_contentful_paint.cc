// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/interaction_contentful_paint.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

InteractionContentfulPaint::InteractionContentfulPaint(
    double start_time,
    DOMHighResTimeStamp render_time,
    uint64_t size,
    DOMHighResTimeStamp load_time,
    const AtomicString& id,
    const String& url,
    Element* element,
    DOMWindow* source,
    uint32_t navigation_id)
    : PerformanceEntry(/*duration=*/0.0,
                       g_empty_atom,
                       start_time,
                       source,
                       navigation_id),
      size_(size),
      render_time_(render_time),
      load_time_(load_time),
      id_(id),
      url_(url),
      element_(element) {}

InteractionContentfulPaint::~InteractionContentfulPaint() = default;

const AtomicString& InteractionContentfulPaint::entryType() const {
  return performance_entry_names::kInteractionContentfulPaint;
}

PerformanceEntryType InteractionContentfulPaint::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kInteractionContentfulPaint;
}

Element* InteractionContentfulPaint::element() const {
  if (!element_ || !element_->isConnected() || element_->IsInShadowTree()) {
    return nullptr;
  }

  // Do not expose |element_| when the document is not 'fully active'.
  const Document& document = element_->GetDocument();
  if (!document.IsActive() || !document.GetFrame()) {
    return nullptr;
  }

  return element_.Get();
}

void InteractionContentfulPaint::BuildJSONValue(
    V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddInteger("size", size_);
  builder.AddNumber("renderTime", render_time_);
  builder.AddNumber("loadTime", load_time_);
  builder.AddString("id", id_);
  builder.AddString("url", url_);
}

void InteractionContentfulPaint::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
