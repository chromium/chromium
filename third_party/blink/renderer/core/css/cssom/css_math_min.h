// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_MIN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_MIN_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_variadic.h"

namespace blink {

struct CSSNumericSumValue;

// Represents the minimum of one or more CSSNumericValues.
// See CSSMathMin.idl for more information about this class.
class CORE_EXPORT CSSMathMin final : public CSSMathVariadic {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The constructor defined in the IDL.
  static CSSMathMin* Create(const HeapVector<CSSNumberish>& args,
                            ExceptionState&);
  // Blink-internal constructor.
  static CSSMathMin* Create(CSSNumericValueVector);

  CSSMathMin(CSSNumericArray* values, const CSSNumericValueType& type)
      : CSSMathVariadic(values, type) {}

  String getOperator() const final { return "min"; }

  // From CSSStyleValue.
  StyleValueType GetType() const final { return CSSStyleValue::kMinType; }

  CSSMathExpressionNode* ToCalcExpressionNode() const final;

 private:
  void BuildCSSText(Nested, ParenLess, StringBuilder&) const final;

  base::Optional<CSSNumericSumValue> SumValue() const final;
  DISALLOW_COPY_AND_ASSIGN(CSSMathMin);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_MIN_H_
