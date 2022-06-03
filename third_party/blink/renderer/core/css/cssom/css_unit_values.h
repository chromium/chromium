// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_UNIT_VALUES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_UNIT_VALUES_H_

#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"

namespace blink {

class CSSUnitValues {
  STATIC_ONLY(CSSUnitValues);

 public:
  // <length>
  static CSSUnitValue* number(double value) {
    return CSSUnitValue::Create(value, CSSPrimitiveValue::UnitType::kNumber);
  }

  static CSSUnitValue* percent(double value) {
    return CSSUnitValue::Create(value,
                                CSSPrimitiveValue::UnitType::kPercentage);
  }

  static CSSUnitValue* em(double value) {
    return CSSUnitValue::Create(value, CSSPrimitiveValue::UnitType::kEms);
  }

  static CSSUnitValue* ex(double value) {
    return CSSUnitValue::Create(value, CSSPrimitiveValue::UnitType::kExs);
  }

  static CSSUnitValue* ch(double value) {
    return CSSUnitValue::Create(value, CSSPrimitiveValue::UnitType::kChs);
  }

  static CSSUnitValue* rem(double value) {
    return CSSUnitValue::Create(value, CSSPrimitiveValue::UnitType::kRems);
  }

  static CSSUnitValue* vw(double value) {
    return CSSUnitValue::Create(value,
                                CSSPrimitiveValue::UnitType::kViewportWidth);
  }

  static CSSUnitValue* vh(double value) {
    return CSSUnitValue::Create(value,
                                CSSPrimitiveValue::UnitType::kViewportHeight);
  }

  static CSSUnitValue* qw(double value) {
    return CSSUnitValue::Create(value,
                                CSSPrimitiveValue::UnitType::kContainerWidth);
  }

  static CSSUnitValue* qh(double value) {
    return CSSUnitValue::Create(value,
                                CSSPrimitiveValue::UnitType::kContainerHeight);
  }

  static CSSUnitValue* qi(double value) {
    return CSSUnitValue::Create(
        value, CSSPrimitiveValue::UnitType::kContainerInlineSize);
  }

  static CSSUnitValue* qb(double value) {
    return CSSUnitValue::Create(
        value, CSSPrimitiveValue::UnitType::kContainerBlockSize);
  }

  static CSSUnitValue* qmin(double value) {
    return CSSUnitValue::Create(value,
                                CSSPrimitiveValue::UnitType::kContainerMin);
  }

  static CSSUnitValue* qmax(double value) {
    return CSSUnitValue::Create(value,
                                CSSPrimitiveValue::UnitType::kContainerMax);
  }

  static CSSUnitValue* vmin(double value) {
    return CSSUnitValue::Create(value,
                                CSSPrimitiveValue::UnitType::kViewportMin);
  }

  static CSSUnitValue* vmax(double value) {
    return CSSUnitValue::Create(value,
                                CSSPrimitiveValue::UnitType::kViewportMax);
  }

  static CSSUnitValue* cm(double value) {
    return CSSUnitValue::Create(value,
                                CSSPrimitiveValue::UnitType::kCentimeters);
  }

  static CSSUnitValue* mm(double value) {
    return CSSUnitValue::Create(value,
                                CSSPrimitiveValue::UnitType::kMillimeters);
  }

  static CSSUnitValue* Q(double value) {
    return CSSUnitValue::Create(
        value, CSSPrimitiveValue::UnitType::kQuarterMillimeters);
  }

  static CSSUnitValue* in(double value) {
    return CSSUnitValue::Create(value, CSSPrimitiveValue::UnitType::kInches);
  }

  static CSSUnitValue* pt(double value) {
    return CSSUnitValue::Create(value, CSSPrimitiveValue::UnitType::kPoints);
  }

  static CSSUnitValue* pc(double value) {
    return CSSUnitValue::Create(value, CSSPrimitiveValue::UnitType::kPicas);
  }

  static CSSUnitValue* px(double value) {
    return CSSUnitValue::Create(value, CSSPrimitiveValue::UnitType::kPixels);
  }

  // <angle>
  static CSSUnitValue* deg(double value) {
    return CSSUnitValue::Create(value, CSSPrimitiveValue::UnitType::kDegrees);
  }

  static CSSUnitValue* grad(double value) {
    return CSSUnitValue::Create(value, CSSPrimitiveValue::UnitType::kGradians);
  }

  static CSSUnitValue* rad(double value) {
    return CSSUnitValue::Create(value, CSSPrimitiveValue::UnitType::kRadians);
  }

  static CSSUnitValue* turn(double value) {
    return CSSUnitValue::Create(value, CSSPrimitiveValue::UnitType::kTurns);
  }

  // <time>
  static CSSUnitValue* s(double value) {
    return CSSUnitValue::Create(value, CSSPrimitiveValue::UnitType::kSeconds);
  }

  static CSSUnitValue* ms(double value) {
    return CSSUnitValue::Create(value,
                                CSSPrimitiveValue::UnitType::kMilliseconds);
  }

  // <frequency>
  static CSSUnitValue* Hz(double value) {
    return CSSUnitValue::Create(value, CSSPrimitiveValue::UnitType::kHertz);
  }

  static CSSUnitValue* kHz(double value) {
    return CSSUnitValue::Create(value, CSSPrimitiveValue::UnitType::kKilohertz);
  }

  // <resolution>
  static CSSUnitValue* dpi(double value) {
    return CSSUnitValue::Create(value,
                                CSSPrimitiveValue::UnitType::kDotsPerInch);
  }

  static CSSUnitValue* dpcm(double value) {
    return CSSUnitValue::Create(
        value, CSSPrimitiveValue::UnitType::kDotsPerCentimeter);
  }

  static CSSUnitValue* dppx(double value) {
    return CSSUnitValue::Create(value,
                                CSSPrimitiveValue::UnitType::kDotsPerPixel);
  }

  // <flex>
  static CSSUnitValue* fr(double value) {
    return CSSUnitValue::Create(value, CSSPrimitiveValue::UnitType::kFraction);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_UNIT_VALUES_H_
