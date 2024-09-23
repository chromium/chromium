/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_transform.h"

#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

SVGTransform::SVGTransform() : transform_type_(SVGTransformType::kUnknown) {}

SVGTransform::SVGTransform(SVGTransformType transform_type,
                           ConstructionMode mode)
    : transform_type_(transform_type) {
  if (mode == kConstructZeroTransform)
    matrix_ = AffineTransform(0, 0, 0, 0, 0, 0);
}

SVGTransform::SVGTransform(const AffineTransform& matrix)
    : SVGTransform(SVGTransformType::kMatrix, 0, gfx::PointF(), matrix) {}

SVGTransform::~SVGTransform() = default;

SVGTransform* SVGTransform::Clone() const {
  return MakeGarbageCollected<SVGTransform>(transform_type_, angle_, center_,
                                            matrix_);
}

SVGPropertyBase* SVGTransform::CloneForAnimation(const String&) const {
  // SVGTransform is never animated.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void SVGTransform::SetMatrix(const AffineTransform& matrix) {
  OnMatrixChange();
  matrix_ = matrix;
}

void SVGTransform::OnMatrixChange() {
  transform_type_ = SVGTransformType::kMatrix;
  angle_ = 0;
}

void SVGTransform::SetTranslate(float tx, float ty) {
  transform_type_ = SVGTransformType::kTranslate;
  angle_ = 0;

  matrix_.MakeIdentity();
  matrix_.Translate(tx, ty);
}

gfx::Vector2dF SVGTransform::Translate() const {
  return gfx::Vector2dF(ClampTo<float>(matrix_.E()),
                        ClampTo<float>(matrix_.F()));
}

void SVGTransform::SetScale(float sx, float sy) {
  transform_type_ = SVGTransformType::kScale;
  angle_ = 0;
  center_ = gfx::PointF();

  matrix_.MakeIdentity();
  matrix_.ScaleNonUniform(sx, sy);
}

gfx::Vector2dF SVGTransform::Scale() const {
  return gfx::Vector2dF(ClampTo<float>(matrix_.A()),
                        ClampTo<float>(matrix_.D()));
}

void SVGTransform::SetRotate(float angle, float cx, float cy) {
  transform_type_ = SVGTransformType::kRotate;
  angle_ = angle;
  center_ = gfx::PointF(cx, cy);

  // TODO: toString() implementation, which can show cx, cy (need to be stored?)
  matrix_.MakeIdentity();
  matrix_.Translate(cx, cy);
  matrix_.Rotate(angle);
  matrix_.Translate(-cx, -cy);
}

void SVGTransform::SetSkewX(float angle) {
  transform_type_ = SVGTransformType::kSkewx;
  angle_ = angle;

  matrix_.MakeIdentity();
  matrix_.SkewX(angle);
}

void SVGTransform::SetSkewY(float angle) {
  transform_type_ = SVGTransformType::kSkewy;
  angle_ = angle;

  matrix_.MakeIdentity();
  matrix_.SkewY(angle);
}

namespace {

const char* TransformTypePrefixForParsing(SVGTransformType type) {
  switch (type) {
    case SVGTransformType::kUnknown:
      return "";
    case SVGTransformType::kMatrix:
      return "matrix(";
    case SVGTransformType::kTranslate:
      return "translate(";
    case SVGTransformType::kScale:
      return "scale(";
    case SVGTransformType::kRotate:
      return "rotate(";
    case SVGTransformType::kSkewx:
      return "skewX(";
    case SVGTransformType::kSkewy:
      return "skewY(";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

gfx::PointF DecomposeRotationCenter(const AffineTransform& matrix,
                                    float angle) {
  const double angle_in_rad = Deg2rad(angle);
  const double cos_angle = std::cos(angle_in_rad);
  const double sin_angle = std::sin(angle_in_rad);
  if (cos_angle == 1)
    return gfx::PointF();
  // Solve for the point <cx, cy> from a matrix on the form:
  //
  // [ a, c, e ] = [ cos(a), -sin(a), cx + (-cx * cos(a)) + (-cy * -sin(a)) ]
  // [ b, d, f ]   [ sin(a),  cos(a), cy + (-cx * sin(a)) + (-cy *  cos(a)) ]
  //
  // => cx = (e * (1 - cos(a)) - f * sin(a)) / (1 - cos(a)) / 2
  //    cy = (e * sin(a) / (1 - cos(a)) + f) / 2
  const double e = matrix.E();
  const double f = matrix.F();
  const double cx = (e * (1 - cos_angle) - f * sin_angle) / (1 - cos_angle) / 2;
  const double cy = (e * sin_angle / (1 - cos_angle) + f) / 2;
  return gfx::PointF(ClampTo<float>(cx), ClampTo<float>(cy));
}

}  // namespace

String SVGTransform::ValueAsString() const {
  std::array<double, 6> arguments;
  size_t argument_count = 0;
  switch (transform_type_) {
    case SVGTransformType::kUnknown:
      return g_empty_string;
    case SVGTransformType::kMatrix: {
      arguments[argument_count++] = matrix_.A();
      arguments[argument_count++] = matrix_.B();
      arguments[argument_count++] = matrix_.C();
      arguments[argument_count++] = matrix_.D();
      arguments[argument_count++] = matrix_.E();
      arguments[argument_count++] = matrix_.F();
      break;
    }
    case SVGTransformType::kTranslate: {
      arguments[argument_count++] = matrix_.E();
      arguments[argument_count++] = matrix_.F();
      break;
    }
    case SVGTransformType::kScale: {
      arguments[argument_count++] = matrix_.A();
      arguments[argument_count++] = matrix_.D();
      break;
    }
    case SVGTransformType::kRotate: {
      arguments[argument_count++] = angle_;

      const gfx::PointF center = DecomposeRotationCenter(matrix_, angle_);
      if (!center.IsOrigin()) {
        arguments[argument_count++] = center.x();
        arguments[argument_count++] = center.y();
      }
      break;
    }
    case SVGTransformType::kSkewx:
      arguments[argument_count++] = angle_;
      break;
    case SVGTransformType::kSkewy:
      arguments[argument_count++] = angle_;
      break;
  }
  DCHECK_LE(argument_count, std::size(arguments));

  StringBuilder builder;
  builder.Append(TransformTypePrefixForParsing(transform_type_));

  for (size_t i = 0; i < argument_count; ++i) {
    if (i)
      builder.Append(' ');
    builder.AppendNumber(arguments[i]);
  }
  builder.Append(')');
  return builder.ToString();
}

void SVGTransform::Add(const SVGPropertyBase*, const SVGElement*) {
  // SVGTransform is not animated by itself.
  NOTREACHED_IN_MIGRATION();
}

void SVGTransform::CalculateAnimatedValue(const SMILAnimationEffectParameters&,
                                          float,
                                          unsigned,
                                          const SVGPropertyBase*,
                                          const SVGPropertyBase*,
                                          const SVGPropertyBase*,
                                          const SVGElement*) {
  // SVGTransform is not animated by itself.
  NOTREACHED_IN_MIGRATION();
}

float SVGTransform::CalculateDistance(const SVGPropertyBase*,
                                      const SVGElement*) const {
  // SVGTransform is not animated by itself.
  NOTREACHED_IN_MIGRATION();

  return -1;
}

}  // namespace blink
