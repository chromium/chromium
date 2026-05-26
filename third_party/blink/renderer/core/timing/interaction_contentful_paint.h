// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_INTERACTION_CONTENTFUL_PAINT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_INTERACTION_CONTENTFUL_PAINT_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class LargestContentfulPaint;

// Exposes the Interaction to Largest Contentful Paint, computed as described in
// https://github.com/WICG/soft-navigations
class CORE_EXPORT InteractionContentfulPaint final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  InteractionContentfulPaint(double start_time,
                             DOMHighResTimeStamp render_time,
                             LargestContentfulPaint* largest_contentful_paint,
                             DOMWindow* source,
                             uint32_t navigation_id,
                             uint64_t interaction_id);
  ~InteractionContentfulPaint() override;

  const AtomicString& entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;

  LargestContentfulPaint* largestContentfulPaint() const {
    return largest_contentful_paint_.Get();
  }
  uint64_t interactionId() const { return interaction_id_; }

  void Trace(Visitor*) const override;

 private:
  void BuildJSONValue(V8ObjectBuilder&) const override;

  Member<LargestContentfulPaint> largest_contentful_paint_;
  uint64_t interaction_id_;
};

template <>
struct DowncastTraits<InteractionContentfulPaint> {
  static bool AllowFrom(const PerformanceEntry& entry) {
    return entry.EntryTypeEnum() ==
           PerformanceEntry::EntryType::kInteractionContentfulPaint;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_INTERACTION_CONTENTFUL_PAINT_H_
