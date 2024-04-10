// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/result_caching_anchor_evaluator.h"

#include "third_party/blink/renderer/core/css/anchor_results.h"

namespace blink {

ResultCachingAnchorEvaluator::ResultCachingAnchorEvaluator(
    AnchorEvaluator* evaluator,
    AnchorResults& results)
    : evaluator_(evaluator), results_(results) {
  results_.Clear();
}

std::optional<LayoutUnit> ResultCachingAnchorEvaluator::Evaluate(
    const AnchorQuery& query,
    const ScopedCSSName* position_anchor,
    const std::optional<InsetAreaOffsets>& inset_area_offsets) {
  if (GetMode() == AnchorScope::Mode::kNone) {
    return std::nullopt;
  }
  // Forward mode to inner evaluator.
  AnchorScope anchor_scope(GetMode(), evaluator_);
  std::optional<LayoutUnit> result =
      evaluator_
          ? evaluator_->Evaluate(query, position_anchor, inset_area_offsets)
          : std::optional<LayoutUnit>();
  results_.Set(GetMode(), query, result);
  return result;
}

std::optional<InsetAreaOffsets>
ResultCachingAnchorEvaluator::ComputeInsetAreaOffsetsForLayout(
    const ScopedCSSName* position_anchor,
    InsetArea inset_area) {
  if (!evaluator_) {
    return std::nullopt;
  }
  return evaluator_->ComputeInsetAreaOffsetsForLayout(position_anchor,
                                                      inset_area);
}

std::optional<PhysicalOffset>
ResultCachingAnchorEvaluator::ComputeAnchorCenterOffsets(
    const ComputedStyleBuilder& builder) {
  if (!evaluator_) {
    return std::nullopt;
  }
  return evaluator_->ComputeAnchorCenterOffsets(builder);
}

}  // namespace blink
