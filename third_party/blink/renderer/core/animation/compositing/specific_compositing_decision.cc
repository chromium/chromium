// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/compositing/specific_compositing_decision.h"

namespace blink {

void CompositingDecisionDetailsMap::AddUnsupportedProperty(
    const PropertyHandle& property) {
  EnsureBucket(SpecificCompositingDecision::kUnsupportedPropertyName)
      .emplace_back(SpecificCompositingDecision::kUnsupportedPropertyName,
                    property);
}

void CompositingDecisionDetailsMap::ResetDetails(
    std::initializer_list<SpecificCompositingDecision> reasons) {
  for (SpecificCompositingDecision reason : reasons) {
    auto it = buckets_.find(reason);
    if (it != buckets_.end()) {
      buckets_.erase(it);
    }
  }
}

CompositingDecisionDetailsMap::Bucket&
CompositingDecisionDetailsMap::EnsureBucket(SpecificCompositingDecision type) {
  return buckets_.insert(type, Bucket()).stored_value->value;
}

}  // namespace blink
