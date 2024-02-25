// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

LargestContentfulPaint::LargestContentfulPaint(
    double start_time,
    DOMHighResTimeStamp render_time,
    uint64_t size,
    DOMHighResTimeStamp load_time,
    DOMHighResTimeStamp first_animated_frame_time,
    const AtomicString& id,
    const String& url,
    Element* element,
    DOMWindow* source,
    bool is_triggered_by_soft_navigation)
    : PerformanceEntry(g_empty_atom,
                       start_time,
                       start_time,
                       source,
                       is_triggered_by_soft_navigation),
      size_(size),
      render_time_(render_time),
      load_time_(load_time),
      first_animated_frame_time_(first_animated_frame_time),
      id_(id),
      url_(url),
      element_(element) {}

LargestContentfulPaint::~LargestContentfulPaint() = default;

const AtomicString& LargestContentfulPaint::entryType() const {
  return performance_entry_names::kLargestContentfulPaint;
}

PerformanceEntryType LargestContentfulPaint::EntryTypeEnum() const {
  return PerformanceEntry::EntryType::kLargestContentfulPaint;
}

Element* LargestContentfulPaint::element() const {
  if (!element_ || !element_->isConnected() || element_->IsInShadowTree())
    return nullptr;

  // Do not expose |element_| when the document is not 'fully active'.
  const Document& document = element_->GetDocument();
  if (!document.IsActive() || !document.GetFrame())
    return nullptr;

  return element_.Get();
}

void LargestContentfulPaint::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddInteger("size", size_);
  builder.AddNumber("renderTime", render_time_);
  builder.AddNumber("loadTime", load_time_);
  builder.AddNumber("firstAnimatedFrameTime", first_animated_frame_time_);
  builder.AddString("id", id_);
  builder.AddString("url", url_);
}

void LargestContentfulPaint::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
