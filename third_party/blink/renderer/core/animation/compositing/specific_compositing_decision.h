// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COMPOSITING_SPECIFIC_COMPOSITING_DECISION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COMPOSITING_SPECIFIC_COMPOSITING_DECISION_H_

#include <cstdint>
#include <initializer_list>
#include <optional>

#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Specific decisions made when checking whether/how an animation should be
// optimized. These are not reported as histograms, or tracked outside of
// tracing, and therefore can be added/removed without concern for
// compatibility. These exist to address the case where the same decision
// histogram bucket is used by multiple code paths, and you want to clear only
// the tracing bucket associated with an individual code path.
enum class SpecificCompositingDecision : uint32_t {
  kUnsupportedPropertyName = 0,
};

struct CORE_EXPORT SpecificCompositingDecisionDetail {
  SpecificCompositingDecision type;
  std::optional<PropertyHandle> property = std::nullopt;
};

class CORE_EXPORT CompositingDecisionDetailsMap
    : public GarbageCollected<CompositingDecisionDetailsMap> {
 public:
  using Bucket = Vector<SpecificCompositingDecisionDetail>;

  void AddUnsupportedProperty(const PropertyHandle& property);
  void ResetDetails(std::initializer_list<SpecificCompositingDecision> reasons);

  const HashMap<SpecificCompositingDecision, Bucket>& Map() const {
    return buckets_;
  }

  void Trace(Visitor*) const {}

 private:
  Bucket& EnsureBucket(SpecificCompositingDecision type);

  HashMap<SpecificCompositingDecision, Bucket> buckets_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COMPOSITING_SPECIFIC_COMPOSITING_DECISION_H_
