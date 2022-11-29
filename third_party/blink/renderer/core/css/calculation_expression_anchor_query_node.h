// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CALCULATION_EXPRESSION_ANCHOR_QUERY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CALCULATION_EXPRESSION_ANCHOR_QUERY_NODE_H_

#include "third_party/blink/renderer/core/style/scoped_css_name.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class CORE_EXPORT CalculationExpressionAnchorQueryNode final
    : public CalculationExpressionNode {
 public:
  static scoped_refptr<const CalculationExpressionAnchorQueryNode> CreateAnchor(
      const ScopedCSSName* name,
      AnchorValue side,
      const Length& fallback);
  static scoped_refptr<const CalculationExpressionAnchorQueryNode>
  CreateAnchorPercentage(const ScopedCSSName* name,
                         float percentage,
                         const Length& fallback);
  static scoped_refptr<const CalculationExpressionAnchorQueryNode>
  CreateAnchorSize(const ScopedCSSName* name,
                   AnchorSizeValue size,
                   const Length& fallback);

  AnchorQueryType Type() const { return type_; }
  const ScopedCSSName* AnchorName() const { return anchor_name_; }
  AnchorValue AnchorSide() const {
    DCHECK_EQ(type_, AnchorQueryType::kAnchor);
    return value_.anchor_side;
  }
  float AnchorSidePercentage() const {
    DCHECK_EQ(type_, AnchorQueryType::kAnchor);
    DCHECK_EQ(AnchorSide(), AnchorValue::kPercentage);
    return side_percentage_;
  }
  float AnchorSidePercentageOrZero() const {
    DCHECK_EQ(type_, AnchorQueryType::kAnchor);
    return AnchorSide() == AnchorValue::kPercentage ? side_percentage_ : 0;
  }
  AnchorSizeValue AnchorSize() const {
    DCHECK_EQ(type_, AnchorQueryType::kAnchorSize);
    return value_.anchor_size;
  }
  const Length& GetFallback() const { return fallback_; }

  // Implement |CalculationExpressionNode|:
  float Evaluate(float max_value, const Length::AnchorEvaluator*) const final;
  bool operator==(const CalculationExpressionNode& other) const final;
  scoped_refptr<const CalculationExpressionNode> Zoom(
      double factor) const final;
  bool IsAnchorQuery() const final { return true; }
  ~CalculationExpressionAnchorQueryNode() final = default;

#if DCHECK_IS_ON()
  ResultType ResolvedResultType() const final {
    return ResultType::kPixelsAndPercent;
  }
#endif

  union AnchorQueryValue {
    AnchorValue anchor_side;
    AnchorSizeValue anchor_size;
  };

  CalculationExpressionAnchorQueryNode(AnchorQueryType type,
                                       const ScopedCSSName* anchor_name,
                                       AnchorQueryValue value,
                                       float side_percentage,
                                       const Length& fallback)
      : type_(type),
        anchor_name_(anchor_name),
        value_(value),
        side_percentage_(side_percentage),
        fallback_(fallback) {
    has_anchor_queries_ = true;
  }

 private:
  AnchorQueryType type_;
  Persistent<const ScopedCSSName> anchor_name_;
  AnchorQueryValue value_;
  float side_percentage_ = 0;  // For AnchorSideValue::kPercentage only
  Length fallback_;
};

template <>
struct DowncastTraits<CalculationExpressionAnchorQueryNode> {
  static bool AllowFrom(const CalculationExpressionNode& node) {
    return node.IsAnchorQuery();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CALCULATION_EXPRESSION_ANCHOR_QUERY_NODE_H_
