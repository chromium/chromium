/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/graphics/gradient.h"

#include <algorithm>
#include <optional>

#include "third_party/blink/renderer/platform/geometry/blend.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings_builder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

namespace blink {

Gradient::Gradient(Type type,
                   GradientSpreadMethod spread_method,
                   ColorInterpolation interpolation,
                   DegenerateHandling degenerate_handling)
    : type_(type),
      spread_method_(spread_method),
      color_interpolation_(interpolation),
      degenerate_handling_(degenerate_handling),
      stops_sorted_(true) {}

Gradient::~Gradient() = default;

static inline bool CompareStops(const Gradient::ColorStop& a,
                                const Gradient::ColorStop& b) {
  return a.stop < b.stop;
}

void Gradient::AddColorStop(const Gradient::ColorStop& stop) {
  if (stops_.empty()) {
    stops_sorted_ = true;
  } else {
    stops_sorted_ = stops_sorted_ && CompareStops(stops_.back(), stop);
  }

  stops_.push_back(stop);
  cached_shader_.reset();
}

void Gradient::AddColorStops(const Vector<Gradient::ColorStop>& stops) {
  for (const auto& stop : stops) {
    AddColorStop(stop);
  }
}

void Gradient::SortStopsIfNecessary() const {
  if (stops_sorted_)
    return;

  stops_sorted_ = true;

  if (!stops_.size())
    return;

  std::stable_sort(stops_.begin(), stops_.end(), CompareStops);
}

static SkColor4f ResolveStopColorWithMissingParams(
    const Color& color,
    const Color& neighbor,
    Color::ColorSpace color_space,
    const cc::ColorFilter* color_filter) {
  std::optional<float> param0 =
      color.Param0IsNone() ? neighbor.Param0() : color.Param0();
  std::optional<float> param1 =
      color.Param1IsNone() ? neighbor.Param1() : color.Param1();
  std::optional<float> param2 =
      color.Param2IsNone() ? neighbor.Param2() : color.Param2();
  std::optional<float> alpha =
      color.AlphaIsNone() ? neighbor.Alpha() : color.Alpha();
  Color resolved_color =
      Color::FromColorSpace(color_space, param0, param1, param2, alpha);
  if (color_filter) {
    return color_filter->FilterColor(
        resolved_color.ToGradientStopSkColor4f(color_space));
  }
  return resolved_color.ToGradientStopSkColor4f(color_space);
}

// Collect sorted stop position and color information into the pos and colors
// buffers, ensuring stops at both 0.0 and 1.0.
// TODO(fmalita): theoretically Skia should provide the same 0.0/1.0 padding
// (making this logic redundant), but in practice there are rendering diffs;
// investigate.
void Gradient::FillSkiaStops(ColorBuffer& colors, OffsetBuffer& pos) const {
  if (stops_.empty()) {
    // A gradient with no stops must be transparent black.
    pos.push_back(WebCoreDoubleToSkScalar(0));
    colors.push_back(SkColors::kTransparent);
  } else if (stops_.front().stop > 0) {
    // Copy the first stop to 0.0. The first stop position may have a slight
    // rounding error, but we don't care in this float comparison, since
    // 0.0 comes through cleanly and people aren't likely to want a gradient
    // with a stop at (0 + epsilon).
    pos.push_back(WebCoreDoubleToSkScalar(0));
    if (color_filter_) {
      colors.push_back(color_filter_->FilterColor(
          stops_.front().color.ToGradientStopSkColor4f(
              color_space_interpolation_space_)));
    } else {
      colors.push_back(stops_.front().color.ToGradientStopSkColor4f(
          color_space_interpolation_space_));
    }
  }

  // Deal with none parameters.
  for (wtf_size_t i = 0; i < stops_.size(); i++) {
    Color color = stops_[i].color;
    color.ConvertToColorSpace(color_space_interpolation_space_);
    if (color.HasNoneParams()) {
      if (i != 0) {
        // Fill left
        pos.push_back(WebCoreDoubleToSkScalar(stops_[i].stop));
        colors.push_back(ResolveStopColorWithMissingParams(
            color, stops_[i - 1].color, color_space_interpolation_space_,
            color_filter_.get()));
      }

      if (i != stops_.size() - 1) {
        // Fill right
        pos.push_back(WebCoreDoubleToSkScalar(stops_[i].stop));
        colors.push_back(ResolveStopColorWithMissingParams(
            color, stops_[i + 1].color, color_space_interpolation_space_,
            color_filter_.get()));
      }
    } else {
      pos.push_back(WebCoreDoubleToSkScalar(stops_[i].stop));
      if (color_filter_) {
        colors.push_back(
            color_filter_->FilterColor(stops_[i].color.ToGradientStopSkColor4f(
                color_space_interpolation_space_)));
      } else {
        colors.push_back(stops_[i].color.ToGradientStopSkColor4f(
            color_space_interpolation_space_));
      }
    }
  }

  // Copy the last stop to 1.0 if needed. See comment above about this float
  // comparison.
  DCHECK(!pos.empty());
  if (pos.back() < 1) {
    pos.push_back(WebCoreDoubleToSkScalar(1));
    colors.push_back(colors.back());
  }
}

SkGradientShader::Interpolation Gradient::ResolveSkInterpolation() const {
  using sk_colorspace = SkGradientShader::Interpolation::ColorSpace;
  using sk_hue_method = SkGradientShader::Interpolation::HueMethod;
  SkGradientShader::Interpolation sk_interpolation;

  bool has_non_legacy_color = false;
  switch (color_space_interpolation_space_) {
    case Color::ColorSpace::kXYZD65:
    case Color::ColorSpace::kXYZD50:
    case Color::ColorSpace::kSRGBLinear:
      sk_interpolation.fColorSpace = sk_colorspace::kSRGBLinear;
      break;
    case Color::ColorSpace::kLab:
      sk_interpolation.fColorSpace = sk_colorspace::kLab;
      break;
    case Color::ColorSpace::kOklab:
      sk_interpolation.fColorSpace = Color::IsBakedGamutMappingEnabled()
                                         ? sk_colorspace::kOKLabGamutMap
                                         : sk_colorspace::kOKLab;
      break;
    case Color::ColorSpace::kLch:
      sk_interpolation.fColorSpace = sk_colorspace::kLCH;
      break;
    case Color::ColorSpace::kOklch:
      sk_interpolation.fColorSpace = Color::IsBakedGamutMappingEnabled()
                                         ? sk_colorspace::kOKLCHGamutMap
                                         : sk_colorspace::kOKLCH;
      break;
    case Color::ColorSpace::kSRGB:
    case Color::ColorSpace::kSRGBLegacy:
      sk_interpolation.fColorSpace = sk_colorspace::kSRGB;
      break;
    case Color::ColorSpace::kHSL:
      sk_interpolation.fColorSpace = sk_colorspace::kHSL;
      break;
    case Color::ColorSpace::kHWB:
      sk_interpolation.fColorSpace = sk_colorspace::kHWB;
      break;
    case Color::ColorSpace::kNone:
      for (const auto& stop : stops_) {
        if (!Color::IsLegacyColorSpace(stop.color.GetColorSpace())) {
          has_non_legacy_color = true;
        }
      }
      if (has_non_legacy_color) {
        // If no colorspace is provided and the gradient is not entirely
        // composed of legacy colors, Oklab is the default interpolation space.
        sk_interpolation.fColorSpace = Color::IsBakedGamutMappingEnabled()
                                           ? sk_colorspace::kOKLabGamutMap
                                           : sk_colorspace::kOKLab;
      } else {
        // TODO(crbug.com/1379462): This should be kSRGB.
        sk_interpolation.fColorSpace = sk_colorspace::kDestination;
      }
      break;
    // We do not yet support interpolation in these spaces.
    case Color::ColorSpace::kDisplayP3:
    case Color::ColorSpace::kA98RGB:
    case Color::ColorSpace::kProPhotoRGB:
    case Color::ColorSpace::kRec2020:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  switch (hue_interpolation_method_) {
    case Color::HueInterpolationMethod::kLonger:
      sk_interpolation.fHueMethod = sk_hue_method::kLonger;
      break;
    case Color::HueInterpolationMethod::kIncreasing:
      sk_interpolation.fHueMethod = sk_hue_method::kIncreasing;
      break;
    case Color::HueInterpolationMethod::kDecreasing:
      sk_interpolation.fHueMethod = sk_hue_method::kDecreasing;
      break;
    default:
      sk_interpolation.fHueMethod = sk_hue_method::kShorter;
  }

  sk_interpolation.fInPremul =
      (color_interpolation_ == ColorInterpolation::kPremultiplied)
          ? SkGradientShader::Interpolation::InPremul::kYes
          : SkGradientShader::Interpolation::InPremul::kNo;

  return sk_interpolation;
}

sk_sp<PaintShader> Gradient::CreateShaderInternal(
    const SkMatrix& local_matrix) {
  SortStopsIfNecessary();
  DCHECK(stops_sorted_);

  ColorBuffer colors;
  colors.reserve(stops_.size());
  OffsetBuffer pos;
  pos.reserve(stops_.size());

  FillSkiaStops(colors, pos);
  DCHECK_GE(colors.size(), 2ul);
  DCHECK_EQ(pos.size(), colors.size());

  SkTileMode tile = SkTileMode::kClamp;
  switch (spread_method_) {
    case kSpreadMethodReflect:
      tile = SkTileMode::kMirror;
      break;
    case kSpreadMethodRepeat:
      tile = SkTileMode::kRepeat;
      break;
    case kSpreadMethodPad:
      tile = SkTileMode::kClamp;
      break;
  }

  if (is_dark_mode_enabled_) {
    for (auto& color : colors) {
      color = EnsureDarkModeFilter().InvertColorIfNeeded(
          color, DarkModeFilter::ElementRole::kBackground);
    }
  }
  sk_sp<PaintShader> shader = CreateShader(
      colors, pos, tile, ResolveSkInterpolation(), local_matrix, colors.back());
  DCHECK(shader);

  return shader;
}

void Gradient::ApplyToFlags(cc::PaintFlags& flags,
                            const SkMatrix& local_matrix,
                            const ImageDrawOptions& draw_options) {
  if (is_dark_mode_enabled_ != draw_options.apply_dark_mode) {
    is_dark_mode_enabled_ = draw_options.apply_dark_mode;
    cached_shader_.reset();
  }
  if (!cached_shader_ || local_matrix != cached_shader_->GetLocalMatrix() ||
      flags.getColorFilter().get() != color_filter_.get()) {
    color_filter_ = flags.getColorFilter();
    flags.setColorFilter(nullptr);
    cached_shader_ = CreateShaderInternal(local_matrix);
  }

  flags.setShader(cached_shader_);

  // Legacy behavior: gradients are always dithered.
  flags.setDither(true);
}

DarkModeFilter& Gradient::EnsureDarkModeFilter() {
  if (!dark_mode_filter_) {
    dark_mode_filter_ =
        std::make_unique<DarkModeFilter>(GetCurrentDarkModeSettings());
  }
  return *dark_mode_filter_;
}

namespace {

class LinearGradient final : public Gradient {
 public:
  LinearGradient(const gfx::PointF& p0,
                 const gfx::PointF& p1,
                 GradientSpreadMethod spread_method,
                 ColorInterpolation interpolation,
                 DegenerateHandling degenerate_handling)
      : Gradient(Type::kLinear,
                 spread_method,
                 interpolation,
                 degenerate_handling),
        p0_(p0),
        p1_(p1) {}

 protected:
  sk_sp<PaintShader> CreateShader(const ColorBuffer& colors,
                                  const OffsetBuffer& pos,
                                  SkTileMode tile_mode,
                                  SkGradientShader::Interpolation interpolation,
                                  const SkMatrix& local_matrix,
                                  SkColor4f fallback_color) const override {
    if (GetDegenerateHandling() == DegenerateHandling::kDisallow &&
        p0_ == p1_) {
      return PaintShader::MakeEmpty();
    }

    SkPoint pts[2] = {FloatPointToSkPoint(p0_), FloatPointToSkPoint(p1_)};
    return PaintShader::MakeLinearGradient(
        pts, colors.data(), pos.data(), static_cast<int>(colors.size()),
        tile_mode, interpolation, 0 /* flags */, &local_matrix, fallback_color);
  }

 private:
  const gfx::PointF p0_;
  const gfx::PointF p1_;
};

class RadialGradient final : public Gradient {
 public:
  RadialGradient(const gfx::PointF& p0,
                 float r0,
                 const gfx::PointF& p1,
                 float r1,
                 float aspect_ratio,
                 GradientSpreadMethod spread_method,
                 ColorInterpolation interpolation,
                 DegenerateHandling degenerate_handling)
      : Gradient(Type::kRadial,
                 spread_method,
                 interpolation,
                 degenerate_handling),
        p0_(p0),
        p1_(p1),
        r0_(r0),
        r1_(r1),
        aspect_ratio_(aspect_ratio) {}

 protected:
  sk_sp<PaintShader> CreateShader(const ColorBuffer& colors,
                                  const OffsetBuffer& pos,
                                  SkTileMode tile_mode,
                                  SkGradientShader::Interpolation interpolation,
                                  const SkMatrix& local_matrix,
                                  SkColor4f fallback_color) const override {
    const SkMatrix* matrix = &local_matrix;
    std::optional<SkMatrix> adjusted_local_matrix;
    if (aspect_ratio_ != 1) {
      // CSS3 elliptical gradients: apply the elliptical scaling at the
      // gradient center point.
      DCHECK(p0_ == p1_);
      adjusted_local_matrix.emplace(local_matrix);
      adjusted_local_matrix->preScale(1, 1 / aspect_ratio_, p0_.x(), p0_.y());
      matrix = &*adjusted_local_matrix;
    }

    // The radii we give to Skia must be positive. If we're given a
    // negative radius, ask for zero instead.
    const SkScalar radius0 = std::max(WebCoreFloatToSkScalar(r0_), 0.0f);
    const SkScalar radius1 = std::max(WebCoreFloatToSkScalar(r1_), 0.0f);

    if (GetDegenerateHandling() == DegenerateHandling::kDisallow &&
        p0_ == p1_ && radius0 == radius1) {
      return PaintShader::MakeEmpty();
    }

    return PaintShader::MakeTwoPointConicalGradient(
        FloatPointToSkPoint(p0_), radius0, FloatPointToSkPoint(p1_), radius1,
        colors.data(), pos.data(), static_cast<int>(colors.size()), tile_mode,
        interpolation, 0 /* flags */, matrix, fallback_color);
  }

 private:
  const gfx::PointF p0_;
  const gfx::PointF p1_;
  const float r0_;
  const float r1_;
  const float aspect_ratio_;  // For elliptical gradient, width / height.
};

class ConicGradient final : public Gradient {
 public:
  ConicGradient(const gfx::PointF& position,
                float rotation,
                float start_angle,
                float end_angle,
                GradientSpreadMethod spread_method,
                ColorInterpolation interpolation,
                DegenerateHandling degenerate_handling)
      : Gradient(Type::kConic,
                 spread_method,
                 interpolation,
                 degenerate_handling),
        position_(position),
        rotation_(rotation),
        start_angle_(start_angle),
        end_angle_(end_angle) {}

 protected:
  sk_sp<PaintShader> CreateShader(const ColorBuffer& colors,
                                  const OffsetBuffer& pos,
                                  SkTileMode tile_mode,
                                  SkGradientShader::Interpolation interpolation,
                                  const SkMatrix& local_matrix,
                                  SkColor4f fallback_color) const override {
    if (GetDegenerateHandling() == DegenerateHandling::kDisallow &&
        start_angle_ == end_angle_) {
      return PaintShader::MakeEmpty();
    }

    // Skia's sweep gradient angles are relative to the x-axis, not the y-axis.
    const float skia_rotation = rotation_ - 90;
    const SkMatrix* matrix = &local_matrix;
    std::optional<SkMatrix> adjusted_local_matrix;
    if (skia_rotation) {
      adjusted_local_matrix.emplace(local_matrix);
      adjusted_local_matrix->preRotate(skia_rotation, position_.x(),
                                       position_.y());
      matrix = &*adjusted_local_matrix;
    }

    return PaintShader::MakeSweepGradient(
        position_.x(), position_.y(), colors.data(), pos.data(),
        static_cast<int>(colors.size()), tile_mode, start_angle_, end_angle_,
        interpolation, 0 /* flags */, matrix, fallback_color);
  }

 private:
  const gfx::PointF position_;  // center point
  const float rotation_;       // global rotation (deg)
  const float start_angle_;    // angle (deg) corresponding to color position 0
  const float end_angle_;      // angle (deg) corresponding to color position 1
};

}  // namespace

scoped_refptr<Gradient> Gradient::CreateLinear(
    const gfx::PointF& p0,
    const gfx::PointF& p1,
    GradientSpreadMethod spread_method,
    ColorInterpolation interpolation,
    DegenerateHandling degenerate_handling) {
  return base::AdoptRef(new LinearGradient(p0, p1, spread_method, interpolation,
                                           degenerate_handling));
}

scoped_refptr<Gradient> Gradient::CreateRadial(
    const gfx::PointF& p0,
    float r0,
    const gfx::PointF& p1,
    float r1,
    float aspect_ratio,
    GradientSpreadMethod spread_method,
    ColorInterpolation interpolation,
    DegenerateHandling degenerate_handling) {
  return base::AdoptRef(new RadialGradient(p0, r0, p1, r1, aspect_ratio,
                                           spread_method, interpolation,
                                           degenerate_handling));
}

scoped_refptr<Gradient> Gradient::CreateConic(
    const gfx::PointF& position,
    float rotation,
    float start_angle,
    float end_angle,
    GradientSpreadMethod spread_method,
    ColorInterpolation interpolation,
    DegenerateHandling degenerate_handling) {
  return base::AdoptRef(new ConicGradient(position, rotation, start_angle,
                                          end_angle, spread_method,
                                          interpolation, degenerate_handling));
}

}  // namespace blink
