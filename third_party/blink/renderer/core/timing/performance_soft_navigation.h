// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_SOFT_NAVIGATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_SOFT_NAVIGATION_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"

namespace blink {
class InteractionContentfulPaint;
class SoftNavigationContext;
class V8ObjectBuilder;

class CORE_EXPORT PerformanceSoftNavigation final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PerformanceSoftNavigation(double start_time,
                            const DOMPaintTimingInfo& paint_timing_info,
                            SoftNavigationContext* context);
  ~PerformanceSoftNavigation() override;

  const AtomicString& entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;

  V8NavigationType navigationType() const {
    return V8NavigationType(navigation_type_);
  }

  uint64_t interactionId() const { return interaction_id_; }

  InteractionContentfulPaint* getLargestInteractionContentfulPaint() const;

  void Trace(Visitor*) const override;

 private:
  void BuildJSONValue(V8ObjectBuilder&) const override;

  const V8NavigationType::Enum navigation_type_;
  const uint64_t interaction_id_;
  const Member<SoftNavigationContext> context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_SOFT_NAVIGATION_H_
