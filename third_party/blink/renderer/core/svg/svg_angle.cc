/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_angle.h"

#include "third_party/blink/renderer/core/svg/animation/smil_animation_effect_parameters.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

template <>
const SVGEnumerationMap& GetEnumerationMap<SVGMarkerOrientType>() {
  static constexpr auto enum_items = std::to_array<const char* const>({
      "auto",
      "angle",
      "auto-start-reverse",
  });
  static const SVGEnumerationMap entries(enum_items, kSVGMarkerOrientAngle);
  return entries;
}

namespace {

class SVGMarkerOrientEnumeration final : public SVGEnumeration {
 public:
  explicit SVGMarkerOrientEnumeration(SVGAngle* angle)
      : SVGEnumeration(kSVGMarkerOrientAngle), angle_(angle) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(angle_);
    SVGEnumeration::Trace(visitor);
  }

 private:
  void NotifyChange() override {
    DCHECK(angle_);
    angle_->OrientTypeChanged();
  }

  Member<SVGAngle> angle_;
};

float ConvertAngleToUnit(SVGAngle::SVGAngleType from_unit,
                         float value,
                         SVGAngle::SVGAngleType to_unit) {
  switch (from_unit) {
    case SVGAngle::kSvgAngletypeTurn:
      switch (to_unit) {
        case SVGAngle::kSvgAngletypeGrad:
          return Turn2grad(value);
        case SVGAngle::kSvgAngletypeUnspecified:
        case SVGAngle::kSvgAngletypeDeg:
          return Turn2deg(value);
        case SVGAngle::kSvgAngletypeRad:
          return Deg2rad(Turn2deg(value));
        case SVGAngle::kSvgAngletypeTurn:
        case SVGAngle::kSvgAngletypeUnknown:
          NOTREACHED();
      }
      break;
    case SVGAngle::kSvgAngletypeRad:
      switch (to_unit) {
        case SVGAngle::kSvgAngletypeGrad:
          return Rad2grad(value);
        case SVGAngle::kSvgAngletypeUnspecified:
        case SVGAngle::kSvgAngletypeDeg:
          return Rad2deg(value);
        case SVGAngle::kSvgAngletypeTurn:
          return Deg2turn(Rad2deg(value));
        case SVGAngle::kSvgAngletypeRad:
        case SVGAngle::kSvgAngletypeUnknown:
          NOTREACHED();
      }
      break;
    case SVGAngle::kSvgAngletypeGrad:
      switch (to_unit) {
        case SVGAngle::kSvgAngletypeRad:
          return Grad2rad(value);
        case SVGAngle::kSvgAngletypeUnspecified:
        case SVGAngle::kSvgAngletypeDeg:
          return Grad2deg(value);
        case SVGAngle::kSvgAngletypeTurn:
          return Grad2turn(value);
        case SVGAngle::kSvgAngletypeGrad:
        case SVGAngle::kSvgAngletypeUnknown:
          NOTREACHED();
      }
      break;
    // Spec: For angles, a unitless value is treated the same as if degrees
    // were specified.
    case SVGAngle::kSvgAngletypeUnspecified:
    case SVGAngle::kSvgAngletypeDeg:
      switch (to_unit) {
        case SVGAngle::kSvgAngletypeRad:
          return Deg2rad(value);
        case SVGAngle::kSvgAngletypeGrad:
          return Deg2grad(value);
        case SVGAngle::kSvgAngletypeTurn:
          return Deg2turn(value);
        case SVGAngle::kSvgAngletypeUnspecified:
        case SVGAngle::kSvgAngletypeDeg:
          return value;
        case SVGAngle::kSvgAngletypeUnknown:
          NOTREACHED();
      }
      break;
    case SVGAngle::kSvgAngletypeUnknown:
      NOTREACHED();
  }
}

float ConvertDegreesToUnit(float degrees, SVGAngle::SVGAngleType unit) {
  switch (unit) {
    case SVGAngle::kSvgAngletypeGrad:
      return Deg2grad(degrees);
    case SVGAngle::kSvgAngletypeRad:
      return Deg2rad(degrees);
    case SVGAngle::kSvgAngletypeTurn:
      return Deg2turn(degrees);
    case SVGAngle::kSvgAngletypeUnspecified:
    case SVGAngle::kSvgAngletypeUnknown:
    case SVGAngle::kSvgAngletypeDeg:
      return degrees;
  }
}

}  // namespace

SVGAngle::SVGAngle()
    : unit_type_(kSvgAngletypeUnspecified),
      value_in_specified_units_(0),
      orient_type_(MakeGarbageCollected<SVGMarkerOrientEnumeration>(this)) {}

SVGAngle::SVGAngle(SVGAngleType unit_type,
                   float value_in_specified_units,
                   SVGMarkerOrientType orient_type)
    : unit_type_(unit_type),
      value_in_specified_units_(value_in_specified_units),
      orient_type_(MakeGarbageCollected<SVGMarkerOrientEnumeration>(this)) {
  orient_type_->SetEnumValue(orient_type);
}

SVGAngle::~SVGAngle() = default;

void SVGAngle::Trace(Visitor* visitor) const {
  visitor->Trace(orient_type_);
  SVGPropertyBase::Trace(visitor);
}

SVGAngle* SVGAngle::Clone() const {
  return MakeGarbageCollected<SVGAngle>(unit_type_, value_in_specified_units_,
                                        OrientTypeValue());
}

float SVGAngle::Value() const {
  switch (unit_type_) {
    case kSvgAngletypeGrad:
      return Grad2deg(value_in_specified_units_);
    case kSvgAngletypeRad:
      return Rad2deg(value_in_specified_units_);
    case kSvgAngletypeTurn:
      return Turn2deg(value_in_specified_units_);
    case kSvgAngletypeUnspecified:
    case kSvgAngletypeUnknown:
    case kSvgAngletypeDeg:
      return value_in_specified_units_;
  }

  NOTREACHED();
}

void SVGAngle::SetValue(float value) {
  NewValueSpecifiedUnits(unit_type_, ConvertDegreesToUnit(value, unit_type_));
}

template <typename CharType>
static SVGAngle::SVGAngleType StringToAngleType(
    base::span<const CharType>& type_string) {
  // If there's no unit given, the angle type is unspecified.
  if (type_string.empty()) {
    return SVGAngle::kSvgAngletypeUnspecified;
  }

  size_t skip = 0;
  SVGAngle::SVGAngleType type = SVGAngle::kSvgAngletypeUnknown;
  if (IsHTMLSpace<CharType>(type_string[0])) {
    type = SVGAngle::kSvgAngletypeUnspecified;
    skip++;
  } else if (type_string.size() >= 3) {
    if (type_string[0] == 'd' && type_string[1] == 'e' &&
        type_string[2] == 'g') {
      type = SVGAngle::kSvgAngletypeDeg;
      skip += 3;
    } else if (type_string[0] == 'r' && type_string[1] == 'a' &&
               type_string[2] == 'd') {
      type = SVGAngle::kSvgAngletypeRad;
      skip += 3;
    } else if (type_string.size() >= 4) {
      if (type_string[0] == 'g' && type_string[1] == 'r' &&
          type_string[2] == 'a' && type_string[3] == 'd') {
        type = SVGAngle::kSvgAngletypeGrad;
        skip += 4;
      } else if (type_string[0] == 't' && type_string[1] == 'u' &&
                 type_string[2] == 'r' && type_string[3] == 'n') {
        type = SVGAngle::kSvgAngletypeTurn;
        skip += 4;
      }
    }
  }

  type_string = type_string.subspan(skip);
  if (!SkipOptionalSVGSpaces(type_string)) {
    return type;
  }

  return SVGAngle::kSvgAngletypeUnknown;
}

String SVGAngle::ValueAsString() const {
  base::span<const char> unit_string;
  switch (unit_type_) {
    case kSvgAngletypeDeg:
      unit_string = base::span_from_cstring("deg");
      break;
    case kSvgAngletypeRad:
      unit_string = base::span_from_cstring("rad");
      break;
    case kSvgAngletypeGrad:
      unit_string = base::span_from_cstring("grad");
      break;
    case kSvgAngletypeTurn:
      unit_string = base::span_from_cstring("turn");
      break;
    case kSvgAngletypeUnspecified:
    case kSvgAngletypeUnknown:
      break;
  }
  StringBuilder builder;
  builder.AppendNumber(value_in_specified_units_);
  builder.Append(base::as_bytes(unit_string));
  return builder.ToString();
}

template <typename CharType>
static SVGParsingError ParseValue(base::span<const CharType> span,
                                  float& value_in_specified_units,
                                  SVGAngle::SVGAngleType& unit_type) {
  const size_t size = span.size();
  if (!ParseNumber(span, value_in_specified_units, kAllowLeadingWhitespace)) {
    return SVGParsingError(SVGParseStatus::kExpectedAngle, size - span.size());
  }

  unit_type = StringToAngleType(span);
  if (unit_type == SVGAngle::kSvgAngletypeUnknown) {
    return SVGParsingError(SVGParseStatus::kExpectedAngle, size - span.size());
  }

  return SVGParseStatus::kNoError;
}

SVGParsingError SVGAngle::SetValueAsString(const String& value) {
  if (value.empty()) {
    NewValueSpecifiedUnits(kSvgAngletypeUnspecified, 0);
    return SVGParseStatus::kNoError;
  }

  if (value == "auto") {
    NewValueSpecifiedUnits(kSvgAngletypeUnspecified, 0);
    orient_type_->SetEnumValue(kSVGMarkerOrientAuto);
    return SVGParseStatus::kNoError;
  }
  if (value == "auto-start-reverse") {
    NewValueSpecifiedUnits(kSvgAngletypeUnspecified, 0);
    orient_type_->SetEnumValue(kSVGMarkerOrientAutoStartReverse);
    return SVGParseStatus::kNoError;
  }

  float value_in_specified_units = 0;
  SVGAngleType unit_type = kSvgAngletypeUnknown;

  SVGParsingError error = VisitCharacters(value, [&](auto chars) {
    return ParseValue(chars, value_in_specified_units, unit_type);
  });
  if (error != SVGParseStatus::kNoError) {
    return error;
  }

  NewValueSpecifiedUnits(unit_type, value_in_specified_units);
  return SVGParseStatus::kNoError;
}

void SVGAngle::NewValueSpecifiedUnits(SVGAngleType unit_type,
                                      float value_in_specified_units) {
  orient_type_->SetEnumValue(kSVGMarkerOrientAngle);
  unit_type_ = unit_type;
  value_in_specified_units_ = value_in_specified_units;
}

void SVGAngle::ConvertToSpecifiedUnits(SVGAngleType new_unit) {
  // Changing to the same unit can only be a no-op if this SVGAngle is
  // currently representing a numeric angle. If it isn't we need to ensure we
  // get the side-effect of updating the associated enumeration.
  if (IsNumeric() && new_unit == unit_type_) {
    return;
  }
  const float new_value =
      ConvertAngleToUnit(unit_type_, value_in_specified_units_, new_unit);
  NewValueSpecifiedUnits(new_unit, new_value);
}

void SVGAngle::Add(const SVGPropertyBase* other, const SVGElement*) {
  auto* other_angle = To<SVGAngle>(other);

  // Only respect by animations, if from and by are both specified in angles
  // (and not, for example, 'auto').
  if (!IsNumeric() || !other_angle->IsNumeric()) {
    return;
  }

  SetValue(Value() + other_angle->Value());
}

void SVGAngle::Assign(const SVGAngle& other) {
  if (other.IsNumeric()) {
    NewValueSpecifiedUnits(other.UnitType(), other.ValueInSpecifiedUnits());
    return;
  }
  value_in_specified_units_ = 0;
  orient_type_->SetEnumValue(other.OrientTypeValue());
}

void SVGAngle::CalculateAnimatedValue(
    const SMILAnimationEffectParameters& parameters,
    float percentage,
    unsigned repeat_count,
    const SVGPropertyBase* from,
    const SVGPropertyBase* to,
    const SVGPropertyBase* to_at_end_of_duration,
    const SVGElement*) {
  auto* from_angle = To<SVGAngle>(from);
  auto* to_angle = To<SVGAngle>(to);

  // We can only interpolate between two SVGAngles with orient-type 'angle',
  // all other cases will use discrete animation.
  if (!from_angle->IsNumeric() || !to_angle->IsNumeric()) {
    Assign(percentage < 0.5f ? *from_angle : *to_angle);
    return;
  }

  float result = ComputeAnimatedNumber(
      parameters, percentage, repeat_count, from_angle->Value(),
      to_angle->Value(), To<SVGAngle>(to_at_end_of_duration)->Value());
  if (parameters.is_additive) {
    result += Value();
  }

  SetValue(result);
}

float SVGAngle::CalculateDistance(const SVGPropertyBase* other,
                                  const SVGElement*) const {
  return fabsf(Value() - To<SVGAngle>(other)->Value());
}

void SVGAngle::OrientTypeChanged() {
  if (IsNumeric()) {
    return;
  }
  unit_type_ = kSvgAngletypeUnspecified;
  value_in_specified_units_ = 0;
}

SVGMarkerOrientType SVGAngle::OrientTypeValue() const {
  return orient_type_->EnumValue<SVGMarkerOrientType>();
}

bool SVGAngle::IsNumeric() const {
  return OrientTypeValue() == kSVGMarkerOrientAngle;
}

}  // namespace blink
