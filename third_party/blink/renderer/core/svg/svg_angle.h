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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ANGLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ANGLE_H_

#include "third_party/blink/renderer/core/svg/properties/svg_property_helper.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration.h"
#include "third_party/blink/renderer/core/svg/svg_parsing_error.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class SVGAngle;
class SVGAngleTearOff;

enum SVGMarkerOrientType {
  kSVGMarkerOrientUnknown = 0,
  kSVGMarkerOrientAuto,
  kSVGMarkerOrientAngle,
  kSVGMarkerOrientAutoStartReverse
};
DECLARE_SVG_ENUM_MAP(SVGMarkerOrientType);

class SVGMarkerOrientEnumeration final
    : public SVGEnumeration<SVGMarkerOrientType> {
 public:
  SVGMarkerOrientEnumeration(SVGAngle*);
  ~SVGMarkerOrientEnumeration() override;

  void Add(SVGPropertyBase*, SVGElement*) override;
  void CalculateAnimatedValue(const SVGAnimateElement&,
                              float,
                              unsigned,
                              SVGPropertyBase*,
                              SVGPropertyBase*,
                              SVGPropertyBase*,
                              SVGElement*) override;
  float CalculateDistance(SVGPropertyBase*, SVGElement*) override;

  void Trace(blink::Visitor*) override;

 private:
  void NotifyChange() override;

  Member<SVGAngle> angle_;
};

class SVGAngle final : public SVGPropertyHelper<SVGAngle> {
 public:
  typedef SVGAngleTearOff TearOffType;

  enum SVGAngleType {
    kSvgAngletypeUnknown = 0,
    kSvgAngletypeUnspecified = 1,
    kSvgAngletypeDeg = 2,
    kSvgAngletypeRad = 3,
    kSvgAngletypeGrad = 4,
    kSvgAngletypeTurn = 5
  };

  SVGAngle();
  SVGAngle(SVGAngleType, float, SVGMarkerOrientType);
  ~SVGAngle() override;

  SVGAngleType UnitType() const { return unit_type_; }

  void SetValue(float);
  float Value() const;

  // Technically speaking, we don't need any bits (it's always the
  // same), but we want SetInitial to be called.
  static constexpr int kInitialValueBits = 1;
  void SetInitial(unsigned) {
    NewValueSpecifiedUnits(kSvgAngletypeUnspecified, 0);
  }

  void SetValueInSpecifiedUnits(float value_in_specified_units) {
    value_in_specified_units_ = value_in_specified_units;
  }
  float ValueInSpecifiedUnits() const { return value_in_specified_units_; }

  void NewValueSpecifiedUnits(SVGAngleType unit_type,
                              float value_in_specified_units);
  void ConvertToSpecifiedUnits(SVGAngleType unit_type);

  SVGEnumeration<SVGMarkerOrientType>* OrientType() {
    return orient_type_.Get();
  }
  const SVGEnumeration<SVGMarkerOrientType>* OrientType() const {
    return orient_type_.Get();
  }
  void OrientTypeChanged();

  // SVGPropertyBase:

  SVGAngle* Clone() const;

  String ValueAsString() const override;
  SVGParsingError SetValueAsString(const String&);

  void Add(SVGPropertyBase*, SVGElement*) override;
  void CalculateAnimatedValue(const SVGAnimateElement&,
                              float percentage,
                              unsigned repeat_count,
                              SVGPropertyBase* from,
                              SVGPropertyBase* to,
                              SVGPropertyBase* to_at_end_of_duration_value,
                              SVGElement* context_element) override;
  float CalculateDistance(SVGPropertyBase* to,
                          SVGElement* context_element) override;

  static AnimatedPropertyType ClassType() { return kAnimatedAngle; }

  void Trace(blink::Visitor*) override;

 private:
  void Assign(const SVGAngle&);

  SVGAngleType unit_type_;
  float value_in_specified_units_;
  Member<SVGMarkerOrientEnumeration> orient_type_;
};

DEFINE_SVG_PROPERTY_TYPE_CASTS(SVGAngle);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ANGLE_H_
