// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_rgb.h"
#include "third_party/blink/renderer/core/css/cssom/cssom_types.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

namespace {

CSSNumericValue* NumberishToColorComponent(const CSSNumberish& input,
                                           ExceptionState& exception_state) {
  CSSNumericValue* value = CSSNumericValue::FromPercentish(input);
  DCHECK(value);
  if (CSSOMTypes::IsCSSStyleValueNumber(*value) ||
      CSSOMTypes::IsCSSStyleValuePercentage(*value)) {
    return value;
  }

  exception_state.ThrowTypeError(
      "Color channel must be a number or percentage.");
  return nullptr;
}

CSSNumericValue* NumberishToAlphaComponent(const CSSNumberish& input,
                                           ExceptionState& exception_state) {
  CSSNumericValue* value = CSSNumericValue::FromPercentish(input);
  DCHECK(value);
  if (CSSOMTypes::IsCSSStyleValuePercentage(*value))
    return value;

  exception_state.ThrowTypeError("Alpha must be a percentage.");
  return nullptr;
}

float ComponentToColorInput(CSSNumericValue* input) {
  if (CSSOMTypes::IsCSSStyleValuePercentage(*input))
    return input->to(CSSPrimitiveValue::UnitType::kPercentage)->value() / 100;
  return input->to(CSSPrimitiveValue::UnitType::kNumber)->value();
}

}  // namespace

CSSRGB::CSSRGB(const Color& input_color) {
  double r, g, b, a;
  input_color.GetRGBA(r, g, b, a);
  r_ = CSSUnitValue::Create(r * 100, CSSPrimitiveValue::UnitType::kPercentage);
  g_ = CSSUnitValue::Create(g * 100, CSSPrimitiveValue::UnitType::kPercentage);
  b_ = CSSUnitValue::Create(b * 100, CSSPrimitiveValue::UnitType::kPercentage);
  alpha_ =
      CSSUnitValue::Create(a * 100, CSSPrimitiveValue::UnitType::kPercentage);
}

CSSRGB::CSSRGB(CSSNumericValue* r,
               CSSNumericValue* g,
               CSSNumericValue* b,
               CSSNumericValue* alpha)
    : r_(r), g_(g), b_(b), alpha_(alpha) {}

CSSRGB* CSSRGB::Create(const CSSNumberish& red,
                       const CSSNumberish& green,
                       const CSSNumberish& blue,
                       const CSSNumberish& alpha,
                       ExceptionState& exception_state) {
  CSSNumericValue* r;
  CSSNumericValue* g;
  CSSNumericValue* b;
  CSSNumericValue* a;

  if (!(r = NumberishToColorComponent(red, exception_state)))
    return nullptr;
  if (!(g = NumberishToColorComponent(green, exception_state)))
    return nullptr;
  if (!(b = NumberishToColorComponent(blue, exception_state)))
    return nullptr;
  if (!(a = NumberishToAlphaComponent(alpha, exception_state)))
    return nullptr;
  return MakeGarbageCollected<CSSRGB>(r, g, b, a);
}

Color CSSRGB::ToColor() const {
  return Color(ComponentToColorInput(r_), ComponentToColorInput(g_),
               ComponentToColorInput(b_), ComponentToColorInput(alpha_));
}

void CSSRGB::setR(const CSSNumberish& red, ExceptionState& exception_state) {
  if (auto* value = NumberishToColorComponent(red, exception_state))
    r_ = value;
}

void CSSRGB::setG(const CSSNumberish& green, ExceptionState& exception_state) {
  if (auto* value = NumberishToColorComponent(green, exception_state))
    g_ = value;
}

void CSSRGB::setB(const CSSNumberish& blue, ExceptionState& exception_state) {
  if (auto* value = NumberishToColorComponent(blue, exception_state))
    b_ = value;
}

void CSSRGB::setAlpha(const CSSNumberish& alpha,
                      ExceptionState& exception_state) {
  if (auto* value = NumberishToAlphaComponent(alpha, exception_state))
    alpha_ = value;
}

}  // namespace blink
