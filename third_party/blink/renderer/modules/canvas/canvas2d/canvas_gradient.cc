/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_gradient.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_color_interpolation_method.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_hue_interpolation_method.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/gradient.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {
class ExecutionContext;

static Color::ColorSpace V8ColorSpaceToColorSpace(
    V8ColorInterpolationMethod v8_color_space) {
  switch (v8_color_space.AsEnum()) {
    case V8ColorInterpolationMethod::Enum::kSRGB:
      return Color::ColorSpace::kSRGB;
    case V8ColorInterpolationMethod::Enum::kHsl:
      return Color::ColorSpace::kHSL;
    case V8ColorInterpolationMethod::Enum::kHwb:
      return Color::ColorSpace::kHWB;
    case V8ColorInterpolationMethod::Enum::kSRGBLinear:
      return Color::ColorSpace::kSRGBLinear;
    case V8ColorInterpolationMethod::Enum::kDisplayP3:
      return Color::ColorSpace::kDisplayP3;
    case V8ColorInterpolationMethod::Enum::kDisplayP3Linear:
      return Color::ColorSpace::kDisplayP3Linear;
    case V8ColorInterpolationMethod::Enum::kA98Rgb:
      return Color::ColorSpace::kA98RGB;
    case V8ColorInterpolationMethod::Enum::kProphotoRgb:
      return Color::ColorSpace::kProPhotoRGB;
    case V8ColorInterpolationMethod::Enum::kRec2020:
      return Color::ColorSpace::kRec2020;
    case V8ColorInterpolationMethod::Enum::kLab:
      return Color::ColorSpace::kLab;
    case V8ColorInterpolationMethod::Enum::kOklab:
      return Color::ColorSpace::kOklab;
    case V8ColorInterpolationMethod::Enum::kLch:
      return Color::ColorSpace::kLch;
    case V8ColorInterpolationMethod::Enum::kOklch:
      return Color::ColorSpace::kOklch;
    case V8ColorInterpolationMethod::Enum::kXyz:
      return Color::ColorSpace::kXYZD50;
    case V8ColorInterpolationMethod::Enum::kXyzD50:
      return Color::ColorSpace::kXYZD50;
    case V8ColorInterpolationMethod::Enum::kXyzD65:
      return Color::ColorSpace::kXYZD65;
  }

  return Color::ColorSpace::kNone;
}

static Color::HueInterpolationMethod
V8HueInterpolationMethodToHueInterpolationMethod(
    V8HueInterpolationMethod v8_hue_method) {
  switch (v8_hue_method.AsEnum()) {
    case V8HueInterpolationMethod::Enum::kShorter:
      return Color::HueInterpolationMethod::kShorter;
    case V8HueInterpolationMethod::Enum::kLonger:
      return Color::HueInterpolationMethod::kLonger;
    case V8HueInterpolationMethod::Enum::kIncreasing:
      return Color::HueInterpolationMethod::kIncreasing;
    case V8HueInterpolationMethod::Enum::kDecreasing:
      return Color::HueInterpolationMethod::kDecreasing;
  }

  return Color::HueInterpolationMethod::kShorter;
}

CanvasGradient::CanvasGradient(const gfx::PointF& p0, const gfx::PointF& p1)
    : gradient_(
          Gradient::CreateLinear(p0,
                                 p1,
                                 Gradient::SpreadMethod::kPad,
                                 Gradient::PremultipliedAlpha::kUnpremultiplied,
                                 Gradient::DegenerateHandling::kDisallow)) {
}

CanvasGradient::CanvasGradient(const gfx::PointF& p0,
                               float r0,
                               const gfx::PointF& p1,
                               float r1)
    : gradient_(
          Gradient::CreateRadial(p0,
                                 r0,
                                 p1,
                                 r1,
                                 1,
                                 Gradient::SpreadMethod::kPad,
                                 Gradient::PremultipliedAlpha::kUnpremultiplied,
                                 Gradient::DegenerateHandling::kDisallow)) {
}

// CanvasRenderingContext2D.createConicGradient only takes one angle argument
// it makes sense to make that rotation here and always make the angles 0 -> 2pi
CanvasGradient::CanvasGradient(float startAngle, const gfx::PointF& center)
    : gradient_(
          Gradient::CreateConic(center,
                                startAngle,
                                0,
                                360,
                                Gradient::SpreadMethod::kPad,
                                Gradient::PremultipliedAlpha::kUnpremultiplied,
                                Gradient::DegenerateHandling::kDisallow)) {}

void CanvasGradient::addColorStop(double value,
                                  const String& color_string,
                                  ExceptionState& exception_state) {
  if (!(value >= 0 && value <= 1.0)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        StrCat({"The provided value (", String::Number(value),
                ") is outside the range (0.0, 1.0)."}));
    return;
  }

  Color color = Color::kTransparent;
  if (!ParseCanvasColorString(color_string, color)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        StrCat({"The value provided ('", color_string,
                "') could not be parsed as a color."}));
    return;
  }

  gradient_->AddColorStop(value, color);
}

void CanvasGradient::setColorInterpolationMethod(
    const V8ColorInterpolationMethod& color_interpolation_method) {
  color_interpolation_method_ = color_interpolation_method;
  gradient_->SetColorInterpolationSpace(
      V8ColorSpaceToColorSpace(color_interpolation_method_),
      V8HueInterpolationMethodToHueInterpolationMethod(
          hue_interpolation_method_));
}

void CanvasGradient::setHueInterpolationMethod(
    const V8HueInterpolationMethod& hue_interpolation_method) {
  hue_interpolation_method_ = hue_interpolation_method;
  gradient_->SetColorInterpolationSpace(
      V8ColorSpaceToColorSpace(color_interpolation_method_),
      V8HueInterpolationMethodToHueInterpolationMethod(
          hue_interpolation_method_));
}

}  // namespace blink
