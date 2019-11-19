// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_SUM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_SUM_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_variadic.h"

namespace blink {

// Represents the sum of one or more CSSNumericValues.
// See CSSMathSum.idl for more information about this class.
class CORE_EXPORT CSSMathSum final : public CSSMathVariadic {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The constructor defined in the IDL.
  static CSSMathSum* Create(const HeapVector<CSSNumberish>& args,
                            ExceptionState&);
  // Blink-internal constructor.
  static CSSMathSum* Create(CSSNumericValueVector);

  CSSMathSum(CSSNumericArray* values, const CSSNumericValueType& type)
      : CSSMathVariadic(values, type) {}

  String getOperator() const final { return "sum"; }

  // From CSSStyleValue.
  StyleValueType GetType() const final { return CSSStyleValue::kSumType; }

  CSSMathExpressionNode* ToCalcExpressionNode() const final;

 private:
  void BuildCSSText(Nested, ParenLess, StringBuilder&) const final;

  base::Optional<CSSNumericSumValue> SumValue() const final;
  DISALLOW_COPY_AND_ASSIGN(CSSMathSum);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_SUM_H_
