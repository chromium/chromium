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

#include "third_party/blink/renderer/platform/transforms/scale_transform_operation.h"

#include "third_party/blink/renderer/platform/geometry/blend.h"

namespace blink {
namespace {
// Return the correct OperationType for a given scale.
TransformOperation::OperationType GetTypeForScale(double x,
                                                  double y,
                                                  double z) {
  // Note: purely due to ordering, we will convert scale(1, 1, 1) to kScaleX.
  // This is fine; they are equivalent.

  if (z != 1 & y == 1 & x == 1)
    return TransformOperation::kScaleZ;

  if (z != 1)
    return TransformOperation::kScale3D;

  if (y == 1)
    return TransformOperation::kScaleX;

  if (x == 1)
    return TransformOperation::kScaleY;

  // Both x and y are non-1, so a 2D scale.
  return TransformOperation::kScale;
}
}  // namespace

TransformOperation* ScaleTransformOperation::Accumulate(
    const TransformOperation& other) {
  DCHECK(other.CanBlendWith(*this));
  const auto& other_op = To<ScaleTransformOperation>(other);
  // Scale parameters are one in the identity transform function so use
  // accumulation for one-based values.
  double new_x = x_ + other_op.x_ - 1;
  double new_y = y_ + other_op.y_ - 1;
  double new_z = z_ + other_op.z_ - 1;
  return MakeGarbageCollected<ScaleTransformOperation>(
      new_x, new_y, new_z, GetTypeForScale(new_x, new_y, new_z));
}

TransformOperation* ScaleTransformOperation::Blend(
    const TransformOperation* from,
    double progress,
    bool blend_to_identity) {
  DCHECK(!from || CanBlendWith(*from));

  if (blend_to_identity) {
    return MakeGarbageCollected<ScaleTransformOperation>(
        blink::Blend(x_, 1.0, progress), blink::Blend(y_, 1.0, progress),
        blink::Blend(z_, 1.0, progress), type_);
  }

  const ScaleTransformOperation* from_op =
      static_cast<const ScaleTransformOperation*>(from);
  double from_x = from_op ? from_op->x_ : 1.0;
  double from_y = from_op ? from_op->y_ : 1.0;
  double from_z = from_op ? from_op->z_ : 1.0;

  TransformOperation::OperationType type;

  CommonPrimitiveForInterpolation(from, type);

  return MakeGarbageCollected<ScaleTransformOperation>(
      blink::Blend(from_x, x_, progress), blink::Blend(from_y, y_, progress),
      blink::Blend(from_z, z_, progress), type);
}

void ScaleTransformOperation::CommonPrimitiveForInterpolation(
    const TransformOperation* from,
    TransformOperation::OperationType& common_type) const {
  bool is_3d = Is3DOperation() || (from && from->Is3DOperation());
  const ScaleTransformOperation* from_op =
      static_cast<const ScaleTransformOperation*>(from);
  TransformOperation::OperationType from_type =
      from_op ? from_op->type_ : type_;

  if (type_ == from_type) {
    common_type = type_;
  } else if (is_3d) {
    common_type = kScale3D;
  } else {
    common_type = kScale;
  }
}

}  // namespace blink
