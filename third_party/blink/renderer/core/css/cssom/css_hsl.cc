// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_hsl.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/core/css/cssom/cssom_types.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

CSSHSL::CSSHSL(const Color& input_color) {
  double h, s, l;
  input_color.GetHSL(h, s, l);
  h_ = CSSUnitValue::Create(h * 360, CSSPrimitiveValue::UnitType::kDegrees);
  s_ = CSSUnitValue::Create(s * 100, CSSPrimitiveValue::UnitType::kPercentage);
  l_ = CSSUnitValue::Create(l * 100, CSSPrimitiveValue::UnitType::kPercentage);

  double a = input_color.Alpha();
  alpha_ =
      CSSUnitValue::Create(a * 100, CSSPrimitiveValue::UnitType::kPercentage);
}

CSSHSL::CSSHSL(CSSNumericValue* h,
               CSSNumericValue* s,
               CSSNumericValue* l,
               CSSNumericValue* alpha)
    : h_(h), s_(s), l_(l), alpha_(alpha) {}

CSSHSL* CSSHSL::Create(CSSNumericValue* hue,
                       const V8CSSNumberish* saturation,
                       const V8CSSNumberish* lightness,
                       const V8CSSNumberish* alpha,
                       ExceptionState& exception_state) {
  if (!CSSOMTypes::IsCSSStyleValueAngle(*hue)) {
    exception_state.ThrowTypeError("Hue must be a CSS angle type.");
    return nullptr;
  }

  CSSNumericValue* s;
  CSSNumericValue* l;
  CSSNumericValue* a;

  if (!(s = ToPercentage(saturation)) || !(l = ToPercentage(lightness)) ||
      !(a = ToPercentage(alpha))) {
    exception_state.ThrowTypeError(
        "Saturation, lightness and alpha must be interpretable as "
        "percentages.");
    return nullptr;
  }

  return MakeGarbageCollected<CSSHSL>(hue, s, l, a);
}

V8CSSNumberish* CSSHSL::s() const {
  return MakeGarbageCollected<V8CSSNumberish>(s_);
}

V8CSSNumberish* CSSHSL::l() const {
  return MakeGarbageCollected<V8CSSNumberish>(l_);
}

V8CSSNumberish* CSSHSL::alpha() const {
  return MakeGarbageCollected<V8CSSNumberish>(alpha_);
}

void CSSHSL::setH(CSSNumericValue* hue, ExceptionState& exception_state) {
  if (CSSOMTypes::IsCSSStyleValueAngle(*hue)) {
    h_ = hue;
  } else {
    exception_state.ThrowTypeError("Hue must be a CSS angle type.");
  }
}

void CSSHSL::setS(const V8CSSNumberish* saturation,
                  ExceptionState& exception_state) {
  if (auto* value = ToPercentage(saturation)) {
    s_ = value;
  } else {
    exception_state.ThrowTypeError(
        "Saturation must be interpretable as a percentage.");
  }
}

void CSSHSL::setL(const V8CSSNumberish* lightness,
                  ExceptionState& exception_state) {
  if (auto* value = ToPercentage(lightness)) {
    l_ = value;
  } else {
    exception_state.ThrowTypeError(
        "Lightness must be interpretable as a percentage.");
  }
}

void CSSHSL::setAlpha(const V8CSSNumberish* alpha,
                      ExceptionState& exception_state) {
  if (auto* value = ToPercentage(alpha)) {
    alpha_ = value;
  } else {
    exception_state.ThrowTypeError(
        "Alpha must be interpretable as a percentage.");
  }
}

Color CSSHSL::ToColor() const {
  return Color::FromHSLA(h_->to(CSSPrimitiveValue::UnitType::kDegrees)->value(),
                         ComponentToColorInput(s_), ComponentToColorInput(l_),
                         ComponentToColorInput(alpha_));
}

}  // namespace blink
