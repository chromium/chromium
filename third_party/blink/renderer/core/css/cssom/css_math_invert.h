// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_INVERT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_INVERT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_value.h"

namespace blink {

// Represents the inverse of a CSSNumericValue.
// See CSSMathInvert.idl for more information about this class.
class CORE_EXPORT CSSMathInvert : public CSSMathValue {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The constructor defined in the IDL.
  static CSSMathInvert* Create(const CSSNumberish& arg) {
    return Create(CSSNumericValue::FromNumberish(arg));
  }
  // Blink-internal constructor
  static CSSMathInvert* Create(CSSNumericValue* value) {
    return MakeGarbageCollected<CSSMathInvert>(
        value, CSSNumericValueType::NegateExponents(value->Type()));
  }

  CSSMathInvert(CSSNumericValue* value, const CSSNumericValueType& type)
      : CSSMathValue(type), value_(value) {}

  String getOperator() const final { return "invert"; }

  void value(CSSNumberish& value) { value.SetCSSNumericValue(value_); }

  // Blink-internal methods
  const CSSNumericValue& Value() const { return *value_; }

  // From CSSStyleValue.
  StyleValueType GetType() const final { return CSSStyleValue::kInvertType; }

  void Trace(Visitor* visitor) override {
    visitor->Trace(value_);
    CSSMathValue::Trace(visitor);
  }

  bool Equals(const CSSNumericValue& other) const final {
    if (other.GetType() != kNegateType)
      return false;

    // We can safely cast here as we know 'other' has the same type as us.
    const auto& other_invert = static_cast<const CSSMathInvert&>(other);
    return value_->Equals(*other_invert.value_);
  }

  CSSMathExpressionNode* ToCalcExpressionNode() const final {
    // TODO(crbug.com/782103): Implement.
    return nullptr;
  }

 private:
  // From CSSNumericValue
  CSSNumericValue* Invert() final { return value_.Get(); }
  base::Optional<CSSNumericSumValue> SumValue() const final;

  void BuildCSSText(Nested, ParenLess, StringBuilder&) const final;

  Member<CSSNumericValue> value_;
  DISALLOW_COPY_AND_ASSIGN(CSSMathInvert);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_INVERT_H_
