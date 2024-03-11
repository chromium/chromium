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
    const AnchorQuery& query) {
  if (GetMode() == AnchorScope::Mode::kNone) {
    return std::nullopt;
  }
  // Forward mode to inner evaluator.
  AnchorScope anchor_scope(GetMode(), evaluator_);
  std::optional<LayoutUnit> result =
      evaluator_ ? evaluator_->Evaluate(query) : std::optional<LayoutUnit>();
  results_.Set(GetMode(), query, result);
  return result;
}

}  // namespace blink
