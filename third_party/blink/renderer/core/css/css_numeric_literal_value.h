// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_NUMERIC_LITERAL_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_NUMERIC_LITERAL_VALUE_H_

#include "third_party/blink/renderer/core/css/css_primitive_value.h"

namespace blink {

// Numeric values that can be expressed as a single unit (or a naked number or
// percentage). The equivalence of CSS Typed OM's |CSSUnitValue| in the
// |CSSValue| class hierarchy.
class CORE_EXPORT CSSNumericLiteralValue : public CSSPrimitiveValue {
 public:
  static CSSNumericLiteralValue* Create(double num, UnitType);

  CSSNumericLiteralValue(double num, UnitType type);

  UnitType GetType() const {
    return static_cast<UnitType>(numeric_literal_unit_type_);
  }

  bool IsAngle() const { return CSSPrimitiveValue::IsAngle(GetType()); }
  bool IsFontRelativeLength() const {
    return GetType() == UnitType::kQuirkyEms || GetType() == UnitType::kEms ||
           GetType() == UnitType::kExs || GetType() == UnitType::kRems ||
           GetType() == UnitType::kChs;
  }
  bool IsQuirkyEms() const { return GetType() == UnitType::kQuirkyEms; }
  bool IsViewportPercentageLength() const {
    return CSSPrimitiveValue::IsViewportPercentageLength(GetType());
  }
  bool IsLength() const { return CSSPrimitiveValue::IsLength(GetType()); }
  bool IsPx() const { return GetType() == UnitType::kPixels; }
  bool IsNumber() const {
    return GetType() == UnitType::kNumber || GetType() == UnitType::kInteger;
  }
  bool IsInteger() const { return GetType() == UnitType::kInteger; }
  bool IsPercentage() const { return GetType() == UnitType::kPercentage; }
  bool IsTime() const { return CSSPrimitiveValue::IsTime(GetType()); }
  bool IsResolution() const {
    return CSSPrimitiveValue::IsResolution(GetType());
  }
  bool IsFlex() const { return CSSPrimitiveValue::IsFlex(GetType()); }

  bool IsZero() const { return !DoubleValue(); }

  bool IsComputationallyIndependent() const;

  double DoubleValue() const { return num_; }
  double ComputeSeconds() const;
  double ComputeDegrees() const;
  double ComputeDotsPerPixel() const;

  double ComputeLengthPx(
      const CSSToLengthConversionData& conversion_data) const;
  bool AccumulateLengthArray(CSSLengthArray& length_array,
                             double multiplier) const;
  void AccumulateLengthUnitTypes(LengthTypeFlags& types) const;

  String CustomCSSText() const;
  bool Equals(const CSSNumericLiteralValue& other) const;

  void TraceAfterDispatch(blink::Visitor* visitor) const;

 private:
  double num_;
};

template <>
struct DowncastTraits<CSSNumericLiteralValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsNumericLiteralValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_NUMERIC_LITERAL_VALUE_H_
