/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CALCULATION_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CALCULATION_VALUE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class CalculationValue;

enum CalcOperator {
  kCalcAdd = '+',
  kCalcSubtract = '-',
  kCalcMultiply = '*',
  kCalcDivide = '/'
};

// The order of this enum should not change since its elements are used as
// indices in the addSubtractResult matrix.
enum CalculationCategory {
  kCalcNumber = 0,
  kCalcLength,
  kCalcPercent,
  kCalcPercentNumber,
  kCalcPercentLength,
  kCalcAngle,
  kCalcTime,
  kCalcFrequency,
  kCalcLengthNumber,
  kCalcPercentLengthNumber,
  kCalcOther
};

class CSSCalcExpressionNode : public GarbageCollected<CSSCalcExpressionNode> {
 public:
  enum Type { kCssCalcPrimitiveValue = 1, kCssCalcBinaryOperation };

  virtual bool IsZero() const = 0;
  virtual double DoubleValue() const = 0;
  virtual double ComputeLengthPx(const CSSToLengthConversionData&) const = 0;
  virtual void AccumulateLengthArray(CSSLengthArray&,
                                     double multiplier) const = 0;
  virtual void AccumulatePixelsAndPercent(const CSSToLengthConversionData&,
                                          PixelsAndPercent&,
                                          float multiplier = 1) const = 0;
  virtual String CustomCSSText() const = 0;
  virtual bool operator==(const CSSCalcExpressionNode& other) const {
    return category_ == other.category_ && is_integer_ == other.is_integer_;
  }
  virtual Type GetType() const = 0;
  virtual const CSSCalcExpressionNode* LeftExpressionNode() const = 0;
  virtual const CSSCalcExpressionNode* RightExpressionNode() const = 0;
  virtual CalcOperator OperatorType() const = 0;

  CalculationCategory Category() const { return category_; }
  virtual CSSPrimitiveValue::UnitType TypeWithCalcResolved() const = 0;
  bool IsInteger() const { return is_integer_; }

  bool IsNestedCalc() const { return is_nested_calc_; }
  void SetIsNestedCalc() { is_nested_calc_ = true; }

  virtual void Trace(blink::Visitor* visitor) {}

 protected:
  CSSCalcExpressionNode(CalculationCategory category, bool is_integer)
      : category_(category), is_integer_(is_integer) {
    DCHECK_NE(category, kCalcOther);
  }

  CalculationCategory category_;
  bool is_integer_;
  bool is_nested_calc_ = false;
};

class CORE_EXPORT CSSCalcValue : public GarbageCollected<CSSCalcValue> {
 public:
  static CSSCalcValue* Create(const CSSParserTokenRange&, ValueRange);
  static CSSCalcValue* Create(CSSCalcExpressionNode*,
                              ValueRange = kValueRangeAll);

  static CSSCalcExpressionNode* CreateExpressionNode(CSSPrimitiveValue*,
                                                     bool is_integer = false);
  static CSSCalcExpressionNode* CreateExpressionNode(CSSCalcExpressionNode*,
                                                     CSSCalcExpressionNode*,
                                                     CalcOperator);
  static CSSCalcExpressionNode* CreateExpressionNode(double pixels,
                                                     double percent);

  scoped_refptr<CalculationValue> ToCalcValue(
      const CSSToLengthConversionData& conversion_data) const {
    PixelsAndPercent value(0, 0);
    expression_->AccumulatePixelsAndPercent(conversion_data, value);
    return CalculationValue::Create(
        value, non_negative_ ? kValueRangeNonNegative : kValueRangeAll);
  }
  CalculationCategory Category() const { return expression_->Category(); }
  bool IsInt() const { return expression_->IsInteger(); }
  double DoubleValue() const;
  bool IsNegative() const { return expression_->DoubleValue() < 0; }
  ValueRange PermittedValueRange() {
    return non_negative_ ? kValueRangeNonNegative : kValueRangeAll;
  }
  double ComputeLengthPx(const CSSToLengthConversionData&) const;
  void AccumulateLengthArray(CSSLengthArray& length_array,
                             double multiplier) const {
    expression_->AccumulateLengthArray(length_array, multiplier);
  }
  CSSCalcExpressionNode* ExpressionNode() const { return expression_.Get(); }

  String CustomCSSText() const;
  bool Equals(const CSSCalcValue&) const;

  void Trace(blink::Visitor* visitor) { visitor->Trace(expression_); }

 private:
  CSSCalcValue(CSSCalcExpressionNode* expression, ValueRange range)
      : expression_(expression),
        non_negative_(range == kValueRangeNonNegative) {}

  double ClampToPermittedRange(double) const;

  const Member<CSSCalcExpressionNode> expression_;
  const bool non_negative_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CALCULATION_VALUE_H_
