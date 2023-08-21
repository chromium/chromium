// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_hwb.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/core/css/cssom/cssom_types.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

CSSHWB::CSSHWB(const Color& input_color) {
  double h, w, b;
  input_color.GetHWB(h, w, b);
  h_ = CSSUnitValue::Create(h * 360, CSSPrimitiveValue::UnitType::kDegrees);
  w_ = CSSUnitValue::Create(w * 100, CSSPrimitiveValue::UnitType::kPercentage);
  b_ = CSSUnitValue::Create(b * 100, CSSPrimitiveValue::UnitType::kPercentage);

  double a = input_color.Alpha();
  alpha_ =
      CSSUnitValue::Create(a * 100, CSSPrimitiveValue::UnitType::kPercentage);
}

CSSHWB::CSSHWB(CSSNumericValue* h,
               CSSNumericValue* w,
               CSSNumericValue* b,
               CSSNumericValue* alpha)
    : h_(h), w_(w), b_(b), alpha_(alpha) {}

CSSHWB* CSSHWB::Create(CSSNumericValue* hue,
                       const V8CSSNumberish* white,
                       const V8CSSNumberish* black,
                       const V8CSSNumberish* alpha,
                       ExceptionState& exception_state) {
  if (!CSSOMTypes::IsCSSStyleValueAngle(*hue)) {
    exception_state.ThrowTypeError("Hue must be a CSS angle type.");
    return nullptr;
  }

  CSSNumericValue* w;
  CSSNumericValue* b;
  CSSNumericValue* a;

  if (!(w = ToPercentage(white)) || !(b = ToPercentage(black)) ||
      !(a = ToPercentage(alpha))) {
    exception_state.ThrowTypeError(
        "Black, white and alpha must be interpretable as percentages.");
    return nullptr;
  }

  return MakeGarbageCollected<CSSHWB>(hue, w, b, a);
}

V8CSSNumberish* CSSHWB::w() const {
  return MakeGarbageCollected<V8CSSNumberish>(w_);
}

V8CSSNumberish* CSSHWB::b() const {
  return MakeGarbageCollected<V8CSSNumberish>(b_);
}

V8CSSNumberish* CSSHWB::alpha() const {
  return MakeGarbageCollected<V8CSSNumberish>(alpha_);
}

void CSSHWB::setH(CSSNumericValue* hue, ExceptionState& exception_state) {
  if (CSSOMTypes::IsCSSStyleValueAngle(*hue)) {
    h_ = hue;
  } else {
    exception_state.ThrowTypeError("Hue must be a CSS angle type.");
  }
}

void CSSHWB::setW(const V8CSSNumberish* white,
                  ExceptionState& exception_state) {
  if (auto* value = ToPercentage(white)) {
    w_ = value;
  } else {
    exception_state.ThrowTypeError(
        "White must be interpretable as a percentage.");
  }
}

void CSSHWB::setB(const V8CSSNumberish* black,
                  ExceptionState& exception_state) {
  if (auto* value = ToPercentage(black)) {
    b_ = value;
  } else {
    exception_state.ThrowTypeError(
        "Black must be interpretable as a percentage.");
  }
}

void CSSHWB::setAlpha(const V8CSSNumberish* alpha,
                      ExceptionState& exception_state) {
  if (auto* value = ToPercentage(alpha)) {
    alpha_ = value;
  } else {
    exception_state.ThrowTypeError(
        "Alpha must be interpretable as a percentage.");
  }
}

Color CSSHWB::ToColor() const {
  return Color::FromHWBA(h_->to(CSSPrimitiveValue::UnitType::kDegrees)->value(),
                         ComponentToColorInput(w_), ComponentToColorInput(b_),
                         ComponentToColorInput(alpha_));
}

}  // namespace blink
