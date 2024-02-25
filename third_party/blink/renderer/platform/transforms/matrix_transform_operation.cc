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

#include "third_party/blink/renderer/platform/transforms/matrix_transform_operation.h"

#include <algorithm>

namespace blink {

TransformOperation* MatrixTransformOperation::Accumulate(
    const TransformOperation& other_op) {
  DCHECK(other_op.IsSameType(*this));
  const auto& other = To<MatrixTransformOperation>(other_op);

  gfx::Transform result = matrix_;
  if (!result.Accumulate(other.matrix_))
    return nullptr;

  return MakeGarbageCollected<MatrixTransformOperation>(result);
}

TransformOperation* MatrixTransformOperation::Blend(
    const TransformOperation* from,
    double progress,
    bool blend_to_identity) {
  DCHECK(!from || CanBlendWith(*from));

  gfx::Transform from_t;
  if (from)
    from_t = To<MatrixTransformOperation>(from)->matrix_;

  gfx::Transform to_t = matrix_;
  if (blend_to_identity)
    std::swap(from_t, to_t);

  if (!to_t.Blend(from_t, progress))
    return nullptr;

  return MakeGarbageCollected<MatrixTransformOperation>(to_t);
}

TransformOperation* MatrixTransformOperation::Zoom(double factor) {
  gfx::Transform m = matrix_;
  m.Zoom(factor);
  return MakeGarbageCollected<MatrixTransformOperation>(m);
}

}  // namespace blink
