// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_VARIADIC_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_VARIADIC_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_array.h"

namespace blink {

// Represents an arithmetic operation with one or more CSSNumericValues.
class CORE_EXPORT CSSMathVariadic : public CSSMathValue {

 public:
  CSSNumericArray* values() { return values_.Get(); }

  const CSSNumericValueVector& NumericValues() const {
    return values_->Values();
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(values_);
    CSSMathValue::Trace(visitor);
  }

  bool Equals(const CSSNumericValue& other) const final {
    if (GetType() != other.GetType())
      return false;

    // We can safely cast here as we know 'other' has the same type as us.
    const auto& other_variadic = static_cast<const CSSMathVariadic&>(other);
    return std::equal(
        NumericValues().begin(), NumericValues().end(),
        other_variadic.NumericValues().begin(),
        other_variadic.NumericValues().end(),
        [](const auto& a, const auto& b) { return a->Equals(*b); });
  }

 protected:
  CSSMathVariadic(CSSNumericArray* values, const CSSNumericValueType& type)
      : CSSMathValue(type), values_(values) {}

  template <class BinaryTypeOperation>
  static CSSNumericValueType TypeCheck(const CSSNumericValueVector& values,
                                       BinaryTypeOperation op,
                                       bool& error) {
    error = false;

    CSSNumericValueType final_type = values.front()->Type();
    for (wtf_size_t i = 1; i < values.size(); i++) {
      final_type = op(final_type, values[i]->Type(), error);
      if (error)
        return final_type;
    }

    return final_type;
  }

 private:
  Member<CSSNumericArray> values_;
  DISALLOW_COPY_AND_ASSIGN(CSSMathVariadic);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_VARIADIC_H_
