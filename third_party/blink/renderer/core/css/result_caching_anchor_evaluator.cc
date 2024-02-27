// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/result_caching_anchor_evaluator.h"

#include "third_party/blink/renderer/core/css/anchor_results.h"
#include "third_party/blink/renderer/core/css/calculation_expression_anchor_query_node.h"
#include "third_party/blink/renderer/core/css/out_of_flow_data.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

ResultCachingAnchorEvaluator::ResultCachingAnchorEvaluator(
    Length::AnchorEvaluator* evaluator,
    AnchorResults& results)
    : evaluator_(evaluator), results_(results) {
  results_.Clear();
}

std::optional<LayoutUnit> ResultCachingAnchorEvaluator::Evaluate(
    const CalculationExpressionNode& node) {
  if (GetMode() == Length::AnchorScope::Mode::kNone) {
    return std::nullopt;
  }
  // Forward mode to inner evaluator.
  Length::AnchorScope anchor_scope(GetMode(), evaluator_);
  std::optional<LayoutUnit> result =
      evaluator_ ? evaluator_->Evaluate(node) : std::optional<LayoutUnit>();
  results_.Set(GetMode(), node, result);
  return result;
}

}  // namespace blink
