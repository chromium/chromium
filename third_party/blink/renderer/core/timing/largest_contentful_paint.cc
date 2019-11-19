// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

LargestContentfulPaint::LargestContentfulPaint(double start_time,
                                               double render_time,
                                               uint64_t size,
                                               double load_time,
                                               const AtomicString& id,
                                               const String& url,
                                               Element* element)
    : PerformanceEntry(g_empty_atom, start_time, start_time),
      size_(size),
      render_time_(render_time),
      load_time_(load_time),
      id_(id),
      url_(url),
      element_(element) {}

LargestContentfulPaint::~LargestContentfulPaint() = default;

AtomicString LargestContentfulPaint::entryType() const {
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

  return element_;
}

void LargestContentfulPaint::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.Add("size", size_);
  builder.Add("renderTime", render_time_);
  builder.Add("loadTime", load_time_);
  builder.Add("id", id_);
  builder.Add("url", url_);
  builder.Add("element", element());
}

void LargestContentfulPaint::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
