// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_timeline_entry_id_generator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

LargestContentfulPaint::LargestContentfulPaint(uint64_t size,
                                               DOMHighResTimeStamp load_time,
                                               const AtomicString& id,
                                               const String& url,
                                               Element* element,
                                               DOMWindow* source)
    : PerformanceEntry(
          /*duration=*/0.0,
          g_empty_atom,
          /*start_time=*/0.0,
          source,
          /*navigation_id=*/
          PerformanceTimelineEntryIdInfo::kNone.web_exposed_id),
      size_(size),
      load_time_(load_time),
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

bool LargestContentfulPaint::IsReadyForReporting() const {
  return navigationId() !=
             PerformanceTimelineEntryIdInfo::kNone.web_exposed_id &&
         HasPaintTimingInfo();
}

void LargestContentfulPaint::FinalizeEntry(
    const DOMPaintTimingInfo& paint_timing_info,
    uint64_t navigation_id) {
  CHECK(!IsReadyForReporting());
  PerformanceEntry::FinalizeEntry(paint_timing_info, navigation_id);
  CHECK(IsReadyForReporting());
}

Element* LargestContentfulPaint::element() const {
  return Performance::CanExposeNode(element_, source()) ? element_.Get()
                                                        : nullptr;
}

void LargestContentfulPaint::BuildJSONValue(V8ObjectBuilder& builder) const {
  PerformanceEntry::BuildJSONValue(builder);
  builder.AddInteger("size", size_);
  builder.AddNumber("renderTime", renderTime());
  builder.AddNumber("loadTime", load_time_);
  builder.AddString("id", id_);
  builder.AddString("url", url_);
}

void LargestContentfulPaint::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  PerformanceEntry::Trace(visitor);
}

}  // namespace blink
