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

#include "third_party/blink/renderer/platform/transforms/perspective_transform_operation.h"

#include <algorithm>
#include <cmath>
#include "third_party/blink/renderer/platform/geometry/blend.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

TransformOperation* PerspectiveTransformOperation::Accumulate(
    const TransformOperation& other) {
  DCHECK(other.IsSameType(*this));
  const auto& other_op = To<PerspectiveTransformOperation>(other);

  // We want to solve:
  //   -1/p + -1/p' == -1/p'', where we know p and p'.
  //
  // This can be rewritten as:
  //   p'' == (p * p') / (p + p')
  std::optional<double> result;
  if (!Perspective()) {
    // In the special case of 'none', p is conceptually infinite, which
    // means p'' equals p' (including if it's also 'none').
    result = other_op.Perspective();
  } else if (!other_op.Perspective()) {
    result = Perspective();
  } else {
    double other_p = other_op.UsedPerspective();
    double p = UsedPerspective();
    result = (p * other_p) / (p + other_p);
  }

  return MakeGarbageCollected<PerspectiveTransformOperation>(result);
}

TransformOperation* PerspectiveTransformOperation::Blend(
    const TransformOperation* from,
    double progress,
    bool blend_to_identity) {
  if (from && !from->IsSameType(*this))
    return this;

  // https://drafts.csswg.org/css-transforms-2/#interpolation-of-transform-functions
  // says that we should run matrix decomposition and then run the rules for
  // interpolation of matrices, but we know what those rules are going to
  // yield, so just do that directly.
  double from_p_inverse, to_p_inverse;
  if (blend_to_identity) {
    from_p_inverse = InverseUsedPerspective();
    to_p_inverse = 0.0;
  } else {
    if (from) {
      const PerspectiveTransformOperation* from_op =
          static_cast<const PerspectiveTransformOperation*>(from);
      from_p_inverse = from_op->InverseUsedPerspective();
    } else {
      from_p_inverse = 0.0;
    }
    to_p_inverse = InverseUsedPerspective();
  }
  double p_inverse = blink::Blend(from_p_inverse, to_p_inverse, progress);
  std::optional<double> p;
  if (p_inverse > 0.0 && std::isnormal(p_inverse)) {
    p = 1.0 / p_inverse;
  }
  return MakeGarbageCollected<PerspectiveTransformOperation>(p);
}

TransformOperation* PerspectiveTransformOperation::Zoom(double factor) {
  if (!p_) {
    return MakeGarbageCollected<PerspectiveTransformOperation>(p_);
  }
  return MakeGarbageCollected<PerspectiveTransformOperation>(*p_ * factor);
}

}  // namespace blink
