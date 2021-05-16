// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_MAX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_MAX_H_

#include "third_party/blink/renderer/core/css/cssom/css_math_variadic.h"

namespace blink {

// Represents the maximum of one or more CSSNumericValues.
// See CSSMathMax.idl for more information about this class.
class CORE_EXPORT CSSMathMax final : public CSSMathVariadic {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The constructor defined in the IDL.
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  static CSSMathMax* Create(const HeapVector<Member<V8CSSNumberish>>& args,
                            ExceptionState& exception_state);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  static CSSMathMax* Create(const HeapVector<CSSNumberish>& args,
                            ExceptionState&);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  // Blink-internal constructor.
  static CSSMathMax* Create(CSSNumericValueVector);

  CSSMathMax(CSSNumericArray* values, const CSSNumericValueType& type)
      : CSSMathVariadic(values, type) {}
  CSSMathMax(const CSSMathMax&) = delete;
  CSSMathMax& operator=(const CSSMathMax&) = delete;

  String getOperator() const final { return "max"; }

  // From CSSStyleValue.
  StyleValueType GetType() const final { return CSSStyleValue::kMaxType; }

  CSSMathExpressionNode* ToCalcExpressionNode() const final;

 private:
  void BuildCSSText(Nested, ParenLess, StringBuilder&) const final;

  absl::optional<CSSNumericSumValue> SumValue() const final;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_MAX_H_
