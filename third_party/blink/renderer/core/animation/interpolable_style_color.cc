// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_style_color.h"

namespace blink {

namespace {

void SetupColorInterpolationSpaces(Color& first, Color& second) {
  // Interpolations are in srgb-legacy if both colors are in srgb-legacy and
  // in oklab otherwise.
  if (first.GetColorSpace() == second.GetColorSpace()) {
    DCHECK(first.GetColorSpace() == Color::ColorSpace::kSRGBLegacy ||
           first.GetColorSpace() == Color::ColorSpace::kOklab);
    return;
  }
  DCHECK(first.GetColorSpace() == Color::ColorSpace::kOklab ||
         second.GetColorSpace() == Color::ColorSpace::kOklab);
  first.ConvertToColorSpace(Color::ColorSpace::kOklab);
  second.ConvertToColorSpace(Color::ColorSpace::kOklab);
}

}  // end anonymous namespace

void InterpolableStyleColor::Interpolate(const InterpolableValue& to,
                                         const double progress,
                                         InterpolableValue& result) const {
  // From and to colors need to be resolved against currentcolor before
  // blending. Since currentcolor has not be resolved yet, we need to defer the
  // process.
  InterpolableStyleColor& result_color = To<InterpolableStyleColor>(result);
  result_color.from_color_ = this;
  result_color.to_color_ = To<InterpolableStyleColor>(to);
  result_color.fraction_ = progress;
  result_color.blend_op_ = BlendOp::kInterpolate;
}

/* static */
void InterpolableStyleColor::Interpolate(const InterpolableValue& from,
                                         const InterpolableValue& to,
                                         double progress,
                                         InterpolableValue& result) {
  const InterpolableStyleColor& from_color =
      from.IsStyleColor() ? To<InterpolableStyleColor>(from)
                          : *InterpolableStyleColor::Create(from);
  const InterpolableStyleColor& to_color =
      to.IsStyleColor() ? To<InterpolableStyleColor>(to)
                        : *InterpolableStyleColor::Create(to);
  from_color.Interpolate(to_color, progress, result);
}

void InterpolableStyleColor::Composite(const BaseInterpolableColor& other,
                                       double fraction) {
  InterpolableValue* clone = RawClone();
  from_color_ = To<InterpolableStyleColor>(*clone);
  to_color_ = To<InterpolableStyleColor>(other);
  fraction_ = fraction;
  blend_op_ = BlendOp::kComposite;
}

void InterpolableStyleColor::Scale(double scale) {
  NOTREACHED_IN_MIGRATION();
}

void InterpolableStyleColor::Add(const InterpolableValue& other) {
  NOTREACHED_IN_MIGRATION();
}

Color InterpolableStyleColor::Resolve(
    const Color& current_color,
    const Color& active_link_color,
    const Color& link_color,
    const Color& text_color,
    mojom::blink::ColorScheme color_scheme) const {
  if (blend_op_ == BlendOp::kBase) {
    DCHECK(!to_color_);
    if (from_color_) {
      // This path is used when promoting an BaseInterpolableColor to an
      // InterpolableStyleColor
      return from_color_->Resolve(current_color, active_link_color, link_color,
                                  text_color, color_scheme);
    }

    // Unresolved color-mix.
    Color resolved = style_color_.Resolve(current_color, color_scheme);
    resolved.ConvertToColorSpace(resolved.GetColorInterpolationSpace());
    return resolved;
  }

  DCHECK(from_color_);
  DCHECK(to_color_);

  Color first = from_color_->Resolve(current_color, active_link_color,
                                     link_color, text_color, color_scheme);
  Color second = to_color_->Resolve(current_color, active_link_color,
                                    link_color, text_color, color_scheme);

  if (blend_op_ == BlendOp::kInterpolate) {
    SetupColorInterpolationSpaces(first, second);
    return Color::InterpolateColors(first.GetColorSpace(), std::nullopt, first,
                                    second, fraction_);
  }

  // Blend with underlying color.
  DCHECK_EQ(blend_op_, BlendOp::kComposite);
  SetupColorInterpolationSpaces(first, second);

  float scale_from = fraction_ * first.Alpha();
  float scale_to = second.Alpha();
  float p0 = first.Param0() * scale_from + second.Param0() * scale_to;
  float p1 = first.Param1() * scale_from + second.Param1() * scale_to;
  float p2 = first.Param2() * scale_from + second.Param2() * scale_to;
  float alpha = scale_from + scale_to;

  if (alpha == 0) {
    return Color::kTransparent;
  }
  if (alpha > 1) {
    alpha = 1;
  }
  return Color::FromColorSpace(first.GetColorSpace(), p0 / alpha, p1 / alpha,
                               p2 / alpha, alpha);
}

}  // namespace blink
