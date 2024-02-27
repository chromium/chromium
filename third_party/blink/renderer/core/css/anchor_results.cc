// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license t_t can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/anchor_results.h"

#include "third_party/blink/renderer/core/css/calculation_expression_anchor_query_node.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

AnchorItem* AnchorItem::Create(Length::AnchorScope::Mode mode,
                               const CalculationExpressionNode& node) {
  const auto& anchor_node = To<CalculationExpressionAnchorQueryNode>(node);

  // Make an AnchorItem for caching purposes.
  if (anchor_node.Type() == CSSAnchorQueryType::kAnchor) {
    float percentage = anchor_node.AnchorSide() == CSSAnchorValue::kPercentage
                           ? anchor_node.AnchorSidePercentage()
                           : 0.0f;
    return MakeGarbageCollected<AnchorItem>(
        mode, CSSAnchorQueryType::kAnchor, &anchor_node.AnchorSpecifier(),
        percentage, anchor_node.AnchorSide());
  }

  return MakeGarbageCollected<AnchorItem>(
      mode, CSSAnchorQueryType::kAnchorSize, &anchor_node.AnchorSpecifier(),
      /* percentage */ 0, anchor_node.AnchorSize());
}

scoped_refptr<const CalculationExpressionNode> AnchorItem::ToExpressionNode()
    const {
  if (query_type_ == CSSAnchorQueryType::kAnchor) {
    if (absl::get<CSSAnchorValue>(value_) == CSSAnchorValue::kPercentage) {
      return CalculationExpressionAnchorQueryNode::CreateAnchorPercentage(
          *anchor_specifier_, percentage_, /* fallback */ Length());
    }
    return CalculationExpressionAnchorQueryNode::CreateAnchor(
        *anchor_specifier_,
        /* side */ absl::get<CSSAnchorValue>(value_), /* fallback */ Length());
  }
  return CalculationExpressionAnchorQueryNode::CreateAnchorSize(
      *anchor_specifier_,
      /* side */ absl::get<CSSAnchorSizeValue>(value_),
      /* fallback */ Length());
}

void AnchorItem::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_specifier_);
}

void AnchorResults::Trace(Visitor* visitor) const {
  visitor->Trace(map_);
}

std::optional<LayoutUnit> AnchorResults::Evaluate(
    const CalculationExpressionNode& node) {
  if (GetMode() == Length::AnchorScope::Mode::kNone) {
    return std::nullopt;
  }
  auto* item = AnchorItem::Create(GetMode(), node);
  AnchorResultMap::const_iterator i = map_.find(item);
  if (i != map_.end()) {
    return i->value;
  }
  // Store the missing item explicitly. This causes subsequent calls
  // to IsAnyResultDifferent to check this query as well.
  map_.Set(item, std::nullopt);
  return std::nullopt;
}

void AnchorResults::Set(Length::AnchorScope::Mode mode,
                        const CalculationExpressionNode& node,
                        std::optional<LayoutUnit> result) {
  map_.Set(AnchorItem::Create(mode, node), result);
}

void AnchorResults::Clear() {
  map_.clear();
}

bool AnchorResults::IsAnyResultDifferent(
    Length::AnchorEvaluator* evaluator) const {
  for (const auto& [key, old_result] : map_) {
    Length::AnchorScope anchor_scope(key->GetMode(), evaluator);
    std::optional<LayoutUnit> new_result =
        evaluator ? evaluator->Evaluate(*key->ToExpressionNode())
                  : std::optional<LayoutUnit>();
    if (new_result != old_result) {
      return true;
    }
  }
  return false;
}

}  // namespace blink
