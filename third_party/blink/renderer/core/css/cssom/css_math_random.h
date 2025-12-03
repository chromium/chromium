// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_RANDOM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_RANDOM_H_

#include <optional>

#include "third_party/blink/renderer/bindings/core/v8/v8_css_math_operator.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/cssom/css_math_value.h"

namespace blink {

struct CSSNumericSumValue;

// Represents CSS random() function.
// See css_math_random.idl for more information about this class.
class CORE_EXPORT CSSMathRandom final : public CSSMathValue {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The constructor defined in the IDL.
  static CSSMathRandom* Create(double base_value,
                               V8CSSNumberish* min,
                               V8CSSNumberish* max,
                               V8CSSNumberish* step,
                               ExceptionState& exception_state);

  // Blink-internal constructor.
  static CSSMathRandom* Create(double base_value,
                               CSSNumericValue* min,
                               CSSNumericValue* max,
                               CSSNumericValue* step = nullptr);

  CSSMathRandom(double base_value,
                CSSNumericValue* min,
                CSSNumericValue* max,
                CSSNumericValue* step,
                const CSSNumericValueType& type)
      : CSSMathValue(type),
        random_base_value_(base_value),
        min_(min),
        max_(max),
        step_(step) {}
  CSSMathRandom(const CSSMathRandom&) = delete;
  CSSMathRandom& operator=(const CSSMathRandom&) = delete;

  // Getters for attributes defined in the IDL.
  CSSNumericValue* minValue() { return min_.Get(); }
  CSSNumericValue* maxValue() { return max_.Get(); }
  CSSNumericValue* stepValue() { return step_.Get(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(min_);
    visitor->Trace(max_);
    visitor->Trace(step_);
    CSSMathValue::Trace(visitor);
  }

  V8CSSMathOperator getOperator() const final {
    return V8CSSMathOperator(V8CSSMathOperator::Enum::kRandom);
  }

  double baseValue();
  CSSNumericValue* min();
  CSSNumericValue* max();
  CSSNumericValue* step();

  // From CSSStyleValue.
  StyleValueType GetType() const final { return CSSStyleValue::kRandomType; }

  bool Equals(const CSSNumericValue& other) const final {
    if (GetType() != other.GetType()) {
      return false;
    }

    // We can safely cast here as we know 'other' has the same type as us.
    const auto& other_variadic = static_cast<const CSSMathRandom&>(other);
    return (min_->Equals(*other_variadic.min_) &&
            max_->Equals(*other_variadic.max_) &&
            step_->Equals(*other_variadic.step_));
  }

  template <class BinaryTypeOperation>
  static CSSNumericValueType TypeCheck(CSSNumericValue* min,
                                       CSSNumericValue* max,
                                       CSSNumericValue* step,
                                       BinaryTypeOperation op,
                                       bool& error) {
    error = false;
    CSSNumericValueType type = op(min->Type(), max->Type(), error);
    if (error || !step) {
      return type;
    }

    return op(type, step->Type(), error);
  }

  CSSMathExpressionNode* ToCalcExpressionNode() const final;

 private:
  void BuildCSSText(Nested, ParenLess, StringBuilder&) const final;

  std::optional<CSSNumericSumValue> SumValue() const final;

  double random_base_value_;
  Member<CSSNumericValue> min_;
  Member<CSSNumericValue> max_;
  Member<CSSNumericValue> step_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_MATH_RANDOM_H_
