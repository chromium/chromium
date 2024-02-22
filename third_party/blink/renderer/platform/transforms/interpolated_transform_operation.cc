/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/transforms/interpolated_transform_operation.h"

namespace blink {

bool InterpolatedTransformOperation::IsEqualAssumingSameType(
    const TransformOperation& o) const {
  const InterpolatedTransformOperation* t =
      static_cast<const InterpolatedTransformOperation*>(&o);
  return progress_ == t->progress_ && from_ == t->from_ && to_ == t->to_;
}

void InterpolatedTransformOperation::Apply(
    gfx::Transform& transform,
    const gfx::SizeF& border_box_size) const {
  gfx::Transform from_transform;
  gfx::Transform to_transform;
  from_.ApplyRemaining(border_box_size, starting_index_, from_transform);
  to_.ApplyRemaining(border_box_size, starting_index_, to_transform);

  if (!to_transform.Blend(from_transform, progress_) && progress_ < 0.5)
    to_transform = from_transform;
  transform.PreConcat(to_transform);
}

TransformOperation* InterpolatedTransformOperation::Blend(
    const TransformOperation* from,
    double progress,
    bool blend_to_identity) {
  DCHECK(!from || CanBlendWith(*from));

  TransformOperations to_operations;
  to_operations.Operations().push_back(this);
  TransformOperations from_operations;
  if (blend_to_identity) {
    return MakeGarbageCollected<InterpolatedTransformOperation>(
        to_operations, from_operations, 0, progress);
  }

  if (from) {
    from_operations.Operations().push_back(
        const_cast<TransformOperation*>(from));
  }
  return MakeGarbageCollected<InterpolatedTransformOperation>(
      from_operations, to_operations, 0, progress);
}

}  // namespace blink
