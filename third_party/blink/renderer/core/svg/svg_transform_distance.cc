/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#include "third_party/blink/renderer/core/svg/svg_transform_distance.h"

#include <math.h>

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

SVGTransformDistance::SVGTransformDistance()
    : transform_type_(SVGTransformType::kUnknown), angle_(0), cx_(0), cy_(0) {}

SVGTransformDistance::SVGTransformDistance(SVGTransformType transform_type,
                                           float angle,
                                           float cx,
                                           float cy,
                                           const AffineTransform& transform)
    : transform_type_(transform_type),
      angle_(angle),
      cx_(cx),
      cy_(cy),
      transform_(transform) {}

SVGTransformDistance::SVGTransformDistance(
    const SVGTransform* from_svg_transform,
    const SVGTransform* to_svg_transform)
    : angle_(0), cx_(0), cy_(0) {
  transform_type_ = from_svg_transform->TransformType();
  DCHECK_EQ(transform_type_, to_svg_transform->TransformType());

  switch (transform_type_) {
    case SVGTransformType::kMatrix:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case SVGTransformType::kUnknown:
      break;
    case SVGTransformType::kRotate: {
      gfx::Vector2dF center_distance = to_svg_transform->RotationCenter() -
                                       from_svg_transform->RotationCenter();
      angle_ = to_svg_transform->Angle() - from_svg_transform->Angle();
      cx_ = center_distance.x();
      cy_ = center_distance.y();
      break;
    }
    case SVGTransformType::kTranslate: {
      gfx::Vector2dF translation_distance =
          to_svg_transform->Translate() - from_svg_transform->Translate();
      transform_.Translate(translation_distance.x(), translation_distance.y());
      break;
    }
    case SVGTransformType::kScale: {
      float scale_x =
          to_svg_transform->Scale().x() - from_svg_transform->Scale().x();
      float scale_y =
          to_svg_transform->Scale().y() - from_svg_transform->Scale().y();
      transform_.ScaleNonUniform(scale_x, scale_y);
      break;
    }
    case SVGTransformType::kSkewx:
    case SVGTransformType::kSkewy:
      angle_ = to_svg_transform->Angle() - from_svg_transform->Angle();
      break;
  }
}

SVGTransformDistance SVGTransformDistance::ScaledDistance(
    float scale_factor) const {
  switch (transform_type_) {
    case SVGTransformType::kMatrix:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case SVGTransformType::kUnknown:
      return SVGTransformDistance();
    case SVGTransformType::kRotate:
      return SVGTransformDistance(transform_type_, angle_ * scale_factor,
                                  cx_ * scale_factor, cy_ * scale_factor,
                                  AffineTransform());
    case SVGTransformType::kScale:
      return SVGTransformDistance(
          transform_type_, angle_ * scale_factor, cx_ * scale_factor,
          cy_ * scale_factor, AffineTransform(transform_).Scale(scale_factor));
    case SVGTransformType::kTranslate: {
      AffineTransform new_transform(transform_);
      new_transform.SetE(transform_.E() * scale_factor);
      new_transform.SetF(transform_.F() * scale_factor);
      return SVGTransformDistance(transform_type_, 0, 0, 0, new_transform);
    }
    case SVGTransformType::kSkewx:
    case SVGTransformType::kSkewy:
      return SVGTransformDistance(transform_type_, angle_ * scale_factor,
                                  cx_ * scale_factor, cy_ * scale_factor,
                                  AffineTransform());
  }

  NOTREACHED_IN_MIGRATION();
  return SVGTransformDistance();
}

SVGTransform* SVGTransformDistance::AddSVGTransforms(const SVGTransform* first,
                                                     const SVGTransform* second,
                                                     unsigned repeat_count) {
  DCHECK_EQ(first->TransformType(), second->TransformType());

  auto* transform = MakeGarbageCollected<SVGTransform>();

  switch (first->TransformType()) {
    case SVGTransformType::kMatrix:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case SVGTransformType::kUnknown:
      return transform;
    case SVGTransformType::kRotate: {
      transform->SetRotate(first->Angle() + second->Angle() * repeat_count,
                           first->RotationCenter().x() +
                               second->RotationCenter().x() * repeat_count,
                           first->RotationCenter().y() +
                               second->RotationCenter().y() * repeat_count);
      return transform;
    }
    case SVGTransformType::kTranslate: {
      float dx =
          first->Translate().x() + second->Translate().x() * repeat_count;
      float dy =
          first->Translate().y() + second->Translate().y() * repeat_count;
      transform->SetTranslate(dx, dy);
      return transform;
    }
    case SVGTransformType::kScale: {
      gfx::Vector2dF scale = second->Scale();
      scale.Scale(repeat_count);
      scale += first->Scale();
      transform->SetScale(scale.x(), scale.y());
      return transform;
    }
    case SVGTransformType::kSkewx:
      transform->SetSkewX(first->Angle() + second->Angle() * repeat_count);
      return transform;
    case SVGTransformType::kSkewy:
      transform->SetSkewY(first->Angle() + second->Angle() * repeat_count);
      return transform;
  }
  NOTREACHED_IN_MIGRATION();
  return transform;
}

SVGTransform* SVGTransformDistance::AddToSVGTransform(
    const SVGTransform* transform) const {
  DCHECK(transform_type_ == transform->TransformType() ||
         transform_type_ == SVGTransformType::kUnknown);

  SVGTransform* new_transform = transform->Clone();

  switch (transform_type_) {
    case SVGTransformType::kMatrix:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case SVGTransformType::kUnknown:
      return MakeGarbageCollected<SVGTransform>();
    case SVGTransformType::kTranslate: {
      gfx::Vector2dF translation = transform->Translate();
      translation += gfx::Vector2dF(ClampTo<float>(transform_.E()),
                                    ClampTo<float>(transform_.F()));
      new_transform->SetTranslate(translation.x(), translation.y());
      return new_transform;
    }
    case SVGTransformType::kScale: {
      gfx::Vector2dF scale = transform->Scale();
      scale += gfx::Vector2dF(ClampTo<float>(transform_.A()),
                              ClampTo<float>(transform_.D()));
      new_transform->SetScale(scale.x(), scale.y());
      return new_transform;
    }
    case SVGTransformType::kRotate: {
      gfx::PointF center = transform->RotationCenter();
      new_transform->SetRotate(transform->Angle() + angle_, center.x() + cx_,
                               center.y() + cy_);
      return new_transform;
    }
    case SVGTransformType::kSkewx:
      new_transform->SetSkewX(transform->Angle() + angle_);
      return new_transform;
    case SVGTransformType::kSkewy:
      new_transform->SetSkewY(transform->Angle() + angle_);
      return new_transform;
  }

  NOTREACHED_IN_MIGRATION();
  return new_transform;
}

float SVGTransformDistance::Distance() const {
  switch (transform_type_) {
    case SVGTransformType::kMatrix:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case SVGTransformType::kUnknown:
      return 0;
    case SVGTransformType::kRotate:
      return sqrtf(angle_ * angle_ + cx_ * cx_ + cy_ * cy_);
    case SVGTransformType::kScale:
      return static_cast<float>(sqrt(transform_.A() * transform_.A() +
                                     transform_.D() * transform_.D()));
    case SVGTransformType::kTranslate:
      return static_cast<float>(sqrt(transform_.E() * transform_.E() +
                                     transform_.F() * transform_.F()));
    case SVGTransformType::kSkewx:
    case SVGTransformType::kSkewy:
      return angle_;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

}  // namespace blink
