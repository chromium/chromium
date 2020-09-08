/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_LENGTH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_LENGTH_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/svg/properties/svg_property.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_parsing_error.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class QualifiedName;

class SVGLengthTearOff;

class CORE_EXPORT SVGLength final : public SVGPropertyBase {
 public:
  typedef SVGLengthTearOff TearOffType;

  // Initial values for SVGLength properties. If adding a new initial value,
  // keep the list sorted within the same unit. The table containing the actual
  // values are in the .cc file.
  enum class Initial {
    kUnitlessZero,
    kPercentMinus10,
    kPercent0,
    kPercent50,
    kPercent100,
    kPercent120,
    kNumber3,
    kNumValues
  };
  static constexpr int kInitialValueBits = 3;

  explicit SVGLength(SVGLengthMode = SVGLengthMode::kOther);
  SVGLength(Initial, SVGLengthMode);
  SVGLength(const CSSPrimitiveValue&, SVGLengthMode);

  void SetInitial(unsigned);

  void Trace(Visitor*) const override;

  SVGLength* Clone() const;
  SVGPropertyBase* CloneForAnimation(const String&) const override;

  CSSPrimitiveValue::UnitType NumericLiteralType() const {
    DCHECK(value_->IsNumericLiteralValue());
    return To<CSSNumericLiteralValue>(*value_).GetType();
  }

  void SetUnitType(CSSPrimitiveValue::UnitType);
  SVGLengthMode UnitMode() const {
    return static_cast<SVGLengthMode>(unit_mode_);
  }

  bool operator==(const SVGLength&) const;
  bool operator!=(const SVGLength& other) const { return !operator==(other); }

  float Value(const SVGLengthContext&) const;
  void SetValue(float, const SVGLengthContext&);
  void SetValueAsNumber(float);

  float ValueInSpecifiedUnits() const { return value_->GetFloatValue(); }
  void SetValueInSpecifiedUnits(float value);

  const CSSPrimitiveValue& AsCSSPrimitiveValue() const { return *value_; }

  // Resolves LengthTypePercentage into a normalized floating point number (full
  // value is 1.0).
  float ValueAsPercentage() const;

  // Returns a number to be used as percentage (so full value is 100)
  float ValueAsPercentage100() const;

  // Scale the input value by this SVGLength. Higher precision than input *
  // valueAsPercentage().
  float ScaleByPercentage(float) const;

  String ValueAsString() const override;
  SVGParsingError SetValueAsString(const String&);

  void NewValueSpecifiedUnits(CSSPrimitiveValue::UnitType,
                              float value_in_specified_units);
  void ConvertToSpecifiedUnits(CSSPrimitiveValue::UnitType,
                               const SVGLengthContext&);

  // Helper functions
  bool IsRelative() const;
  bool IsFontRelative() const {
    // TODO(crbug.com/979895): This is the result of a refactoring, which might
    // have revealed an existing bug with calculated lengths. Investigate.
    return value_->IsNumericLiteralValue() &&
           To<CSSNumericLiteralValue>(*value_).IsFontRelativeLength();
  }
  bool IsCalculated() const { return value_->IsCalculated(); }
  bool IsPercentage() const { return value_->IsPercentage(); }

  bool IsNegativeNumericLiteral() const;
  bool IsZero() const { return value_->GetFloatValue() == 0; }

  static SVGLengthMode LengthModeForAnimatedLengthAttribute(
      const QualifiedName&);
  static bool NegativeValuesForbiddenForAnimatedLengthAttribute(
      const QualifiedName&);

  void Add(const SVGPropertyBase*, const SVGElement*) override;
  void CalculateAnimatedValue(
      const SMILAnimationEffectParameters&,
      float percentage,
      unsigned repeat_count,
      const SVGPropertyBase* from,
      const SVGPropertyBase* to,
      const SVGPropertyBase* to_at_end_of_duration_value,
      const SVGElement* context_element) override;
  float CalculateDistance(const SVGPropertyBase* to,
                          const SVGElement* context_element) const override;

  static AnimatedPropertyType ClassType() { return kAnimatedLength; }
  AnimatedPropertyType GetType() const override { return ClassType(); }

 private:
  Member<const CSSPrimitiveValue> value_;
  unsigned unit_mode_ : 2;
};

template <>
struct DowncastTraits<SVGLength> {
  static bool AllowFrom(const SVGPropertyBase& value) {
    return value.GetType() == SVGLength::ClassType();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_LENGTH_H_
