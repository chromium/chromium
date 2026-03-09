// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_ENTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_ENTRY_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"

namespace blink {
class InteractionContentfulPaint;
class V8ObjectBuilder;

class CORE_EXPORT SoftNavigationEntry final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  SoftNavigationEntry(
      AtomicString name,
      double start_time,
      const DOMPaintTimingInfo& paint_timing_info,
      DOMWindow* source,
      uint32_t navigation_id,
      V8NavigationType::Enum navigation_type,
      uint64_t interaction_id,
      InteractionContentfulPaint* largest_interaction_contentful_paint);
  ~SoftNavigationEntry() override;

  const AtomicString& entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;

  V8NavigationType navigationType() const {
    return V8NavigationType(navigation_type_);
  }

  uint64_t interactionId() const { return interaction_id_; }

  InteractionContentfulPaint* largestInteractionContentfulPaint() const {
    return largest_interaction_contentful_paint_.Get();
  }

  void Trace(Visitor*) const override;

 private:
  void BuildJSONValue(V8ObjectBuilder&) const override;

  const V8NavigationType::Enum navigation_type_;
  const uint64_t interaction_id_;
  Member<InteractionContentfulPaint> largest_interaction_contentful_paint_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_ENTRY_H_
