/*
 * Copyright (C) 2010 University of Szeged
 * Copyright (C) 2010 Zoltan Herczeg
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/filters/fe_lighting.h"

#include "base/types/optional_util.h"
#include "third_party/blink/renderer/platform/graphics/filters/distant_light_source.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/filters/point_light_source.h"
#include "third_party/blink/renderer/platform/graphics/filters/spot_light_source.h"
#include "third_party/skia/include/core/SkPoint3.h"

namespace blink {

FELighting::FELighting(Filter* filter,
                       LightingType lighting_type,
                       const Color& lighting_color,
                       float surface_scale,
                       float diffuse_constant,
                       float specular_constant,
                       float specular_exponent,
                       scoped_refptr<LightSource> light_source)
    : FilterEffect(filter),
      lighting_type_(lighting_type),
      light_source_(std::move(light_source)),
      lighting_color_(lighting_color),
      surface_scale_(surface_scale),
      diffuse_constant_(std::max(diffuse_constant, 0.0f)),
      specular_constant_(std::max(specular_constant, 0.0f)),
      specular_exponent_(ClampTo(specular_exponent, 1.0f, 128.0f)) {}

Color FELighting::LightingColor() const {
  return lighting_color_;
}

bool FELighting::SetLightingColor(const Color& lighting_color) {
  if (lighting_color_ == lighting_color)
    return false;
  lighting_color_ = lighting_color;
  return true;
}

float FELighting::SurfaceScale() const {
  return surface_scale_;
}

bool FELighting::SetSurfaceScale(float surface_scale) {
  if (surface_scale_ == surface_scale)
    return false;
  surface_scale_ = surface_scale;
  return true;
}

sk_sp<PaintFilter> FELighting::CreateImageFilter() {
  if (!light_source_)
    return CreateTransparentBlack();
  std::optional<PaintFilter::CropRect> crop_rect = GetCropRect();
  const PaintFilter::CropRect* rect = base::OptionalToPtr(crop_rect);
  Color light_color = AdaptColorToOperatingInterpolationSpace(lighting_color_);
  sk_sp<PaintFilter> input(paint_filter_builder::Build(
      InputEffect(0), OperatingInterpolationSpace()));
  switch (light_source_->GetType()) {
    case kLsDistant: {
      DistantLightSource* distant_light_source =
          static_cast<DistantLightSource*>(light_source_.get());
      float azimuth_rad = Deg2rad(distant_light_source->Azimuth());
      float elevation_rad = Deg2rad(distant_light_source->Elevation());
      const SkPoint3 direction = SkPoint3::Make(
          cosf(azimuth_rad) * cosf(elevation_rad),
          sinf(azimuth_rad) * cosf(elevation_rad), sinf(elevation_rad));
      // TODO(crbug/1308932): Remove FromColor and make all SkColor4f.
      return sk_make_sp<LightingDistantPaintFilter>(
          GetLightingType(), direction, SkColor4f::FromColor(light_color.Rgb()),
          surface_scale_, GetFilterConstant(), specular_exponent_,
          std::move(input), rect);
    }
    case kLsPoint: {
      PointLightSource* point_light_source =
          static_cast<PointLightSource*>(light_source_.get());
      const gfx::Point3F position = point_light_source->GetPosition();
      const SkPoint3 sk_position =
          SkPoint3::Make(position.x(), position.y(), position.z());
      // TODO(crbug/1308932): Remove FromColor and make all SkColor4f.
      return sk_make_sp<LightingPointPaintFilter>(
          GetLightingType(), sk_position,
          SkColor4f::FromColor(light_color.Rgb()), surface_scale_,
          GetFilterConstant(), specular_exponent_, std::move(input), rect);
    }
    case kLsSpot: {
      SpotLightSource* spot_light_source =
          static_cast<SpotLightSource*>(light_source_.get());
      const SkPoint3 location =
          SkPoint3::Make(spot_light_source->GetPosition().x(),
                         spot_light_source->GetPosition().y(),
                         spot_light_source->GetPosition().z());
      const SkPoint3 target = SkPoint3::Make(spot_light_source->PointsAt().x(),
                                             spot_light_source->PointsAt().y(),
                                             spot_light_source->PointsAt().z());
      float specular_exponent = spot_light_source->SpecularExponent();
      float limiting_cone_angle = spot_light_source->LimitingConeAngle();
      if (!limiting_cone_angle || limiting_cone_angle > 90 ||
          limiting_cone_angle < -90)
        limiting_cone_angle = 90;
      // TODO(crbug/1308932): Remove FromColor and make all SkColor4f.
      return sk_make_sp<LightingSpotPaintFilter>(
          GetLightingType(), location, target, specular_exponent,
          limiting_cone_angle, SkColor4f::FromColor(light_color.Rgb()),
          surface_scale_, GetFilterConstant(), specular_exponent_,
          std::move(input), rect);
    }
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

PaintFilter::LightingType FELighting::GetLightingType() {
  return specular_constant_ > 0 ? PaintFilter::LightingType::kSpecular
                                : PaintFilter::LightingType::kDiffuse;
}

float FELighting::GetFilterConstant() {
  return GetLightingType() == PaintFilter::LightingType::kSpecular
             ? specular_constant_
             : diffuse_constant_;
}

}  // namespace blink
