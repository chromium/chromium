// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/invalidation/rule_invalidation_data_tracer.h"

namespace blink {

RuleInvalidationDataTracer::RuleInvalidationDataTracer(
    const RuleInvalidationData& rule_invalidation_data)
    : RuleInvalidationDataVisitor(rule_invalidation_data) {}

void RuleInvalidationDataTracer::TraceInvalidationSetsForSelector(
    const CSSSelector& selector) {
  CollectFeaturesFromSelector(selector, /*style_scope=*/nullptr);
}

}  // namespace blink
