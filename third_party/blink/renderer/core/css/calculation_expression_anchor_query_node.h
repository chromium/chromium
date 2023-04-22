// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CALCULATION_EXPRESSION_ANCHOR_QUERY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CALCULATION_EXPRESSION_ANCHOR_QUERY_NODE_H_

#include "third_party/blink/renderer/core/css/css_anchor_query_enums.h"
#include "third_party/blink/renderer/core/style/scoped_css_name.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class AnchorSpecifierValue;

class CORE_EXPORT CalculationExpressionAnchorQueryNode final
    : public CalculationExpressionNode {
 public:
  static scoped_refptr<const CalculationExpressionAnchorQueryNode> CreateAnchor(
      const AnchorSpecifierValue& anchor_specifier,
      CSSAnchorValue side,
      const Length& fallback);
  static scoped_refptr<const CalculationExpressionAnchorQueryNode>
  CreateAnchorPercentage(const AnchorSpecifierValue& anchor_specifier,
                         float percentage,
                         const Length& fallback);
  static scoped_refptr<const CalculationExpressionAnchorQueryNode>
  CreateAnchorSize(const AnchorSpecifierValue& anchor_specifier,
                   CSSAnchorSizeValue size,
                   const Length& fallback);

  CSSAnchorQueryType Type() const { return type_; }
  const AnchorSpecifierValue& AnchorSpecifier() const {
    return *anchor_specifier_;
  }
  CSSAnchorValue AnchorSide() const {
    DCHECK_EQ(type_, CSSAnchorQueryType::kAnchor);
    return value_.anchor_side;
  }
  float AnchorSidePercentage() const {
    DCHECK_EQ(type_, CSSAnchorQueryType::kAnchor);
    DCHECK_EQ(AnchorSide(), CSSAnchorValue::kPercentage);
    return side_percentage_;
  }
  float AnchorSidePercentageOrZero() const {
    DCHECK_EQ(type_, CSSAnchorQueryType::kAnchor);
    return AnchorSide() == CSSAnchorValue::kPercentage ? side_percentage_ : 0;
  }
  CSSAnchorSizeValue AnchorSize() const {
    DCHECK_EQ(type_, CSSAnchorQueryType::kAnchorSize);
    return value_.anchor_size;
  }
  const Length& GetFallback() const { return fallback_; }

  // Implement |CalculationExpressionNode|:
  float Evaluate(float max_value, const Length::AnchorEvaluator*) const final;
  bool Equals(const CalculationExpressionNode& other) const final;
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
    CSSAnchorValue anchor_side;
    CSSAnchorSizeValue anchor_size;
  };

  CalculationExpressionAnchorQueryNode(
      CSSAnchorQueryType type,
      const AnchorSpecifierValue& anchor_specifier,
      AnchorQueryValue value,
      float side_percentage,
      const Length& fallback)
      : type_(type),
        anchor_specifier_(anchor_specifier),
        value_(value),
        side_percentage_(side_percentage),
        fallback_(fallback) {
    has_anchor_queries_ = true;
  }

 private:
  CSSAnchorQueryType type_;
  Persistent<const AnchorSpecifierValue> anchor_specifier_;
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
