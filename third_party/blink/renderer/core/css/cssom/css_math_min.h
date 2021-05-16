// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_MIN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_MIN_H_

#include "third_party/blink/renderer/core/css/cssom/css_math_variadic.h"

namespace blink {

struct CSSNumericSumValue;

// Represents the minimum of one or more CSSNumericValues.
// See CSSMathMin.idl for more information about this class.
class CORE_EXPORT CSSMathMin final : public CSSMathVariadic {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The constructor defined in the IDL.
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  static CSSMathMin* Create(const HeapVector<Member<V8CSSNumberish>>& args,
                            ExceptionState& exception_state);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  static CSSMathMin* Create(const HeapVector<CSSNumberish>& args,
                            ExceptionState&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  // Blink-internal constructor.
  static CSSMathMin* Create(CSSNumericValueVector);

  CSSMathMin(CSSNumericArray* values, const CSSNumericValueType& type)
      : CSSMathVariadic(values, type) {}
  CSSMathMin(const CSSMathMin&) = delete;
  CSSMathMin& operator=(const CSSMathMin&) = delete;

  String getOperator() const final { return "min"; }

  // From CSSStyleValue.
  StyleValueType GetType() const final { return CSSStyleValue::kMinType; }

  CSSMathExpressionNode* ToCalcExpressionNode() const final;

 private:
  void BuildCSSText(Nested, ParenLess, StringBuilder&) const final;

  absl::optional<CSSNumericSumValue> SumValue() const final;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_MIN_H_
