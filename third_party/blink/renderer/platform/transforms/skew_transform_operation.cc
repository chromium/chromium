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

#include "third_party/blink/renderer/platform/transforms/skew_transform_operation.h"

#include "third_party/blink/renderer/platform/geometry/blend.h"

namespace blink {

TransformOperation* SkewTransformOperation::Accumulate(
    const TransformOperation& other) {
  DCHECK(other.CanBlendWith(*this));
  const auto& skew_other = To<SkewTransformOperation>(other);
  return MakeGarbageCollected<SkewTransformOperation>(
      angle_x_ + skew_other.angle_x_, angle_y_ + skew_other.angle_y_, type_);
}

TransformOperation* SkewTransformOperation::Blend(
    const TransformOperation* from,
    double progress,
    bool blend_to_identity) {
  DCHECK(!from || CanBlendWith(*from));

  if (blend_to_identity) {
    return MakeGarbageCollected<SkewTransformOperation>(
        blink::Blend(angle_x_, 0.0, progress),
        blink::Blend(angle_y_, 0.0, progress), type_);
  }

  const SkewTransformOperation* from_op =
      static_cast<const SkewTransformOperation*>(from);
  double from_angle_x = from_op ? from_op->angle_x_ : 0;
  double from_angle_y = from_op ? from_op->angle_y_ : 0;
  return MakeGarbageCollected<SkewTransformOperation>(
      blink::Blend(from_angle_x, angle_x_, progress),
      blink::Blend(from_angle_y, angle_y_, progress), type_);
}

}  // namespace blink
