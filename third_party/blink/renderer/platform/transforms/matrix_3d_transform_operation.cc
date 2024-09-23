/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#include "third_party/blink/renderer/platform/transforms/matrix_3d_transform_operation.h"

#include "third_party/blink/renderer/platform/transforms/rotation.h"
#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/quaternion.h"

#include <algorithm>

namespace blink {

TransformOperation* Matrix3DTransformOperation::Accumulate(
    const TransformOperation& other_op) {
  DCHECK(other_op.IsSameType(*this));
  const auto& other = To<Matrix3DTransformOperation>(other_op);

  gfx::Transform result = matrix_;
  if (!result.Accumulate(other.matrix_))
    return nullptr;

  return MakeGarbageCollected<Matrix3DTransformOperation>(result);
}

TransformOperation* Matrix3DTransformOperation::Blend(
    const TransformOperation* from,
    double progress,
    bool blend_to_identity) {
  DCHECK(!from || CanBlendWith(*from));

  gfx::Transform from_t;
  if (from)
    from_t = To<Matrix3DTransformOperation>(from)->matrix_;

  gfx::Transform to_t = matrix_;
  if (blend_to_identity)
    std::swap(from_t, to_t);

  if (!to_t.Blend(from_t, progress))
    return nullptr;

  return MakeGarbageCollected<Matrix3DTransformOperation>(to_t);
}

TransformOperation* Matrix3DTransformOperation::Zoom(double factor) {
  gfx::Transform result = matrix_;
  result.Zoom(factor);
  return MakeGarbageCollected<Matrix3DTransformOperation>(result);
}

}  // namespace blink
