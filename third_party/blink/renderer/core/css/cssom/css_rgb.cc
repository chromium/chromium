// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_rgb.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

CSSRGB::CSSRGB(const Color& input_color, Color::ColorSpace color_space) {
  Color rgb_color = input_color;
  rgb_color.ConvertToColorSpace(color_space);

  double r = rgb_color.Param0();
  double g = rgb_color.Param1();
  double b = rgb_color.Param2();

  auto rgb_unit_type = CSSPrimitiveValue::UnitType::kUnknown;
  if (color_space == Color::ColorSpace::kSRGBLegacy) {
    rgb_unit_type = CSSPrimitiveValue::UnitType::kNumber;
  } else {
    CHECK_EQ(color_space, Color::ColorSpace::kSRGB);
    r *= 100.0;
    g *= 100.0;
    b *= 100.0;
    rgb_unit_type = CSSPrimitiveValue::UnitType::kPercentage;
  }

  r_ = CSSUnitValue::Create(r, rgb_unit_type);
  g_ = CSSUnitValue::Create(g, rgb_unit_type);
  b_ = CSSUnitValue::Create(b, rgb_unit_type);
  alpha_ = CSSUnitValue::Create(rgb_color.Alpha() * 100.0,
                                CSSPrimitiveValue::UnitType::kPercentage);
}

CSSRGB::CSSRGB(CSSNumericValue* r,
               CSSNumericValue* g,
               CSSNumericValue* b,
               CSSNumericValue* alpha)
    : r_(r), g_(g), b_(b), alpha_(alpha) {}

CSSRGB* CSSRGB::Create(const V8CSSNumberish* red,
                       const V8CSSNumberish* green,
                       const V8CSSNumberish* blue,
                       const V8CSSNumberish* alpha,
                       ExceptionState& exception_state) {
  CSSNumericValue* r;
  CSSNumericValue* g;
  CSSNumericValue* b;
  CSSNumericValue* a;

  if (!(r = ToNumberOrPercentage(red)) || !(g = ToNumberOrPercentage(green)) ||
      !(b = ToNumberOrPercentage(blue))) {
    exception_state.ThrowTypeError(
        "Color channel must be interpretable as a number or a percentage.");
    return nullptr;
  }
  if (!(a = ToPercentage(alpha))) {
    exception_state.ThrowTypeError(
        "Alpha must be interpretable as a percentage.");
    return nullptr;
  }
  return MakeGarbageCollected<CSSRGB>(r, g, b, a);
}

V8CSSNumberish* CSSRGB::r() const {
  return MakeGarbageCollected<V8CSSNumberish>(r_);
}

V8CSSNumberish* CSSRGB::g() const {
  return MakeGarbageCollected<V8CSSNumberish>(g_);
}

V8CSSNumberish* CSSRGB::b() const {
  return MakeGarbageCollected<V8CSSNumberish>(b_);
}

V8CSSNumberish* CSSRGB::alpha() const {
  return MakeGarbageCollected<V8CSSNumberish>(alpha_);
}

void CSSRGB::setR(const V8CSSNumberish* red, ExceptionState& exception_state) {
  if (auto* value = ToNumberOrPercentage(red)) {
    r_ = value;
  } else {
    exception_state.ThrowTypeError(
        "Color channel must be interpretable as a number or a percentage.");
  }
}

void CSSRGB::setG(const V8CSSNumberish* green,
                  ExceptionState& exception_state) {
  if (auto* value = ToNumberOrPercentage(green)) {
    g_ = value;
  } else {
    exception_state.ThrowTypeError(
        "Color channel must be interpretable as a number or a percentage.");
  }
}

void CSSRGB::setB(const V8CSSNumberish* blue, ExceptionState& exception_state) {
  if (auto* value = ToNumberOrPercentage(blue)) {
    b_ = value;
  } else {
    exception_state.ThrowTypeError(
        "Color channel must be interpretable as a number or a percentage.");
  }
}

void CSSRGB::setAlpha(const V8CSSNumberish* alpha,
                      ExceptionState& exception_state) {
  if (auto* value = ToPercentage(alpha)) {
    alpha_ = value;
  } else {
    exception_state.ThrowTypeError(
        "Alpha must be interpretable as a percentage.");
  }
}

Color CSSRGB::ToColor() const {
  return Color::FromRGBAFloat(
      ComponentToColorInput(r_), ComponentToColorInput(g_),
      ComponentToColorInput(b_), ComponentToColorInput(alpha_));
}

}  // namespace blink
