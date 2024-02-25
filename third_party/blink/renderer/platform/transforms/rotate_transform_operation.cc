/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 *
 */

#include "third_party/blink/renderer/platform/transforms/rotate_transform_operation.h"

#include "third_party/blink/renderer/platform/geometry/blend.h"

namespace blink {
namespace {
TransformOperation::OperationType GetTypeForRotation(const Rotation& rotation) {
  float x = rotation.axis.x();
  float y = rotation.axis.y();
  float z = rotation.axis.z();
  if (x && !y && !z)
    return TransformOperation::kRotateX;
  if (y && !x && !z)
    return TransformOperation::kRotateY;
  if (z && !x && !y)
    return TransformOperation::kRotateZ;
  return TransformOperation::kRotate3D;
}
}  // namespace

bool RotateTransformOperation::IsEqualAssumingSameType(
    const TransformOperation& other) const {
  const auto& other_rotation = To<RotateTransformOperation>(other).rotation_;
  return rotation_.axis == other_rotation.axis &&
         rotation_.angle == other_rotation.angle;
}

bool RotateTransformOperation::GetCommonAxis(const RotateTransformOperation* a,
                                             const RotateTransformOperation* b,
                                             gfx::Vector3dF& result_axis,
                                             double& result_angle_a,
                                             double& result_angle_b) {
  return Rotation::GetCommonAxis(a ? a->rotation_ : Rotation(),
                                 b ? b->rotation_ : Rotation(), result_axis,
                                 result_angle_a, result_angle_b);
}

TransformOperation* RotateTransformOperation::Accumulate(
    const TransformOperation& other) {
  DCHECK(IsMatchingOperationType(other.GetType()));
  Rotation new_rotation =
      Rotation::Add(rotation_, To<RotateTransformOperation>(other).rotation_);
  return MakeGarbageCollected<RotateTransformOperation>(
      new_rotation, GetTypeForRotation(new_rotation));
}

TransformOperation* RotateTransformOperation::Blend(
    const TransformOperation* from,
    double progress,
    bool blend_to_identity) {
  DCHECK(!from || CanBlendWith(*from));

  if (blend_to_identity)
    return MakeGarbageCollected<RotateTransformOperation>(
        Rotation(Axis(), Angle() * (1 - progress)), type_);

  // Optimize for single axis rotation
  if (!from)
    return MakeGarbageCollected<RotateTransformOperation>(
        Rotation(Axis(), Angle() * progress), type_);

  // Apply spherical linear interpolation. Rotate around a common axis if
  // possible. Otherwise, convert rotations to 4x4 matrix representations and
  // interpolate the matrix decompositions. The 'from' and 'to' transforms can
  // be of different types (based on axis), but must both have equivalent
  // rotate3d representations.
  DCHECK(from->PrimitiveType() == OperationType::kRotate3D);
  OperationType type =
      from->IsSameType(*this) ? type_ : OperationType::kRotate3D;
  const auto& from_rotate = To<RotateTransformOperation>(*from);
  return MakeGarbageCollected<RotateTransformOperation>(
      Rotation::Slerp(from_rotate.rotation_, rotation_, progress), type);
}

RotateAroundOriginTransformOperation::RotateAroundOriginTransformOperation(
    double angle,
    double origin_x,
    double origin_y)
    : RotateTransformOperation(Rotation(gfx::Vector3dF(0, 0, 1), angle),
                               kRotateAroundOrigin),
      origin_x_(origin_x),
      origin_y_(origin_y) {}

void RotateAroundOriginTransformOperation::Apply(
    gfx::Transform& transform,
    const gfx::SizeF& box_size) const {
  transform.Translate(origin_x_, origin_y_);
  RotateTransformOperation::Apply(transform, box_size);
  transform.Translate(-origin_x_, -origin_y_);
}

bool RotateAroundOriginTransformOperation::IsEqualAssumingSameType(
    const TransformOperation& other) const {
  const auto& other_rotate = To<RotateAroundOriginTransformOperation>(other);
  const Rotation& other_rotation = other_rotate.rotation_;
  return rotation_.axis == other_rotation.axis &&
         rotation_.angle == other_rotation.angle &&
         origin_x_ == other_rotate.origin_x_ &&
         origin_y_ == other_rotate.origin_y_;
}

TransformOperation* RotateAroundOriginTransformOperation::Blend(
    const TransformOperation* from,
    double progress,
    bool blend_to_identity) {
  DCHECK(!from || CanBlendWith(*from));

  if (blend_to_identity) {
    return MakeGarbageCollected<RotateAroundOriginTransformOperation>(
        Angle() * (1 - progress), origin_x_, origin_y_);
  }
  if (!from) {
    return MakeGarbageCollected<RotateAroundOriginTransformOperation>(
        Angle() * progress, origin_x_, origin_y_);
  }
  const auto& from_rotate = To<RotateAroundOriginTransformOperation>(*from);
  return MakeGarbageCollected<RotateAroundOriginTransformOperation>(
      blink::Blend(from_rotate.Angle(), Angle(), progress),
      blink::Blend(from_rotate.origin_x_, origin_x_, progress),
      blink::Blend(from_rotate.origin_y_, origin_y_, progress));
}

TransformOperation* RotateAroundOriginTransformOperation::Zoom(double factor) {
  return MakeGarbageCollected<RotateAroundOriginTransformOperation>(
      Angle(), origin_x_ * factor, origin_y_ * factor);
}

}  // namespace blink
