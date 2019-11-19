// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_FUNCTION_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_FUNCTION_VALUE_H_

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"

namespace blink {

// Numeric values that involve math functions (calc(), min(), max(), etc). This
// is the equivalence of CSS Typed OM's |CSSMathValue| in the |CSSValue| class
// hierarchy.
class CORE_EXPORT CSSMathFunctionValue : public CSSPrimitiveValue {
 public:
  static CSSMathFunctionValue* Create(const Length&, float zoom);
  static CSSMathFunctionValue* Create(const CSSMathExpressionNode*,
                                      ValueRange = kValueRangeAll);

  CSSMathFunctionValue(const CSSMathExpressionNode* expression,
                       ValueRange range);

  const CSSMathExpressionNode* ExpressionNode() const { return expression_; }

  scoped_refptr<CalculationValue> ToCalcValue(
      const CSSToLengthConversionData& conversion_data) const;

  bool MayHaveRelativeUnit() const;

  CalculationCategory Category() const { return expression_->Category(); }
  bool IsAngle() const { return Category() == kCalcAngle; }
  bool IsLength() const { return Category() == kCalcLength; }
  bool IsNumber() const { return Category() == kCalcNumber; }
  bool IsPercentage() const { return Category() == kCalcPercent; }
  bool IsTime() const { return Category() == kCalcTime; }

  bool IsPx() const;

  bool IsInt() const { return expression_->IsInteger(); }
  bool IsNegative() const { return expression_->DoubleValue() < 0; }
  ValueRange PermittedValueRange() const {
    return IsNonNegative() ? kValueRangeNonNegative : kValueRangeAll;
  }

  // When |false|, comparisons between percentage values can be resolved without
  // providing a reference value (e.g., min(10%, 20%) == 10%). When |true|, the
  // result depends on the sign of the reference value (e.g., when referring to
  // a negative value, min(10%, 20%) == 20%).
  // Note: 'background-position' property allows negative reference values.
  bool AllowsNegativePercentageReference() const {
    return allows_negative_percentage_reference_;
  }
  void SetAllowsNegativePercentageReference() {
    // TODO(crbug.com/825895): So far, 'background-position' is the only
    // property that allows resolving a percentage against a negative value. If
    // we have more of such properties, we should instead pass an additional
    // argument to ask the parser to set this flag when constructing |this|.
    allows_negative_percentage_reference_ = true;
  }

  bool IsZero() const;

  bool IsComputationallyIndependent() const;

  // TODO(crbug.com/979895): The semantics of this function is still not very
  // clear. Do not add new callers before further refactoring and cleanups.
  // |DoubleValue()| can be called only when the the math expression can be
  // resolved into a single numeric value *without any type conversion* (e.g.,
  // between px and em). Otherwise, it hits a DCHECK.
  double DoubleValue() const;

  double ComputeSeconds() const;
  double ComputeDegrees() const;
  double ComputeLengthPx(
      const CSSToLengthConversionData& conversion_data) const;

  bool AccumulateLengthArray(CSSLengthArray& length_array,
                             double multiplier) const;
  Length ConvertToLength(
      const CSSToLengthConversionData& conversion_data) const;

  void AccumulateLengthUnitTypes(LengthTypeFlags& types) const {
    expression_->AccumulateLengthUnitTypes(types);
  }

  String CustomCSSText() const;
  bool Equals(const CSSMathFunctionValue& other) const;

  void TraceAfterDispatch(blink::Visitor* visitor);

 private:
  bool IsNonNegative() const { return is_non_negative_math_function_; }

  double ClampToPermittedRange(double) const;

  Member<const CSSMathExpressionNode> expression_;
};

template <>
struct DowncastTraits<CSSMathFunctionValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsMathFunctionValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_FUNCTION_VALUE_H_
