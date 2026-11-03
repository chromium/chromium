// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_LARGEST_CONTENTFUL_PAINT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_LARGEST_CONTENTFUL_PAINT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

// Exposes the Largest Contenful Paint, computed as described in
// https://github.com/WICG/LargestContentfulPaint.
class CORE_EXPORT LargestContentfulPaint final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  LargestContentfulPaint(uint64_t size,
                         DOMHighResTimeStamp load_time,
                         const AtomicString& id,
                         const String& url,
                         Element* element,
                         DOMWindow* source);
  ~LargestContentfulPaint() override;

  const AtomicString& entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;
  DOMHighResTimeStamp startTime() const override { return renderTime(); }
  bool IsReadyForReporting() const override;
  void FinalizeEntry(const DOMPaintTimingInfo&,
                     uint64_t navigation_id) override;

  uint64_t size() const { return size_; }
  // "The renderTime attribute must return the default paint timestamp given
  // this’s paint timing info."
  DOMHighResTimeStamp renderTime() const {
    return GetPaintTimingInfo().presentation_time;
  }
  DOMHighResTimeStamp loadTime() const { return load_time_; }
  const AtomicString& id() const { return id_; }
  const String& url() const { return url_; }
  Element* element() const;

  void Trace(Visitor*) const override;

 private:
  void BuildJSONValue(V8ObjectBuilder&) const override;

  uint64_t size_;
  DOMHighResTimeStamp load_time_;
  AtomicString id_;
  String url_;
  WeakMember<Element> element_;
};

template <>
struct DowncastTraits<LargestContentfulPaint> {
  static bool AllowFrom(const PerformanceEntry& entry) {
    return entry.EntryTypeEnum() ==
           PerformanceEntry::EntryType::kLargestContentfulPaint;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_LARGEST_CONTENTFUL_PAINT_H_
