/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_point_tear_off.h"

#include "third_party/blink/renderer/core/svg/svg_matrix_tear_off.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

SVGPointTearOff::SVGPointTearOff(SVGPoint* target,
                                 SVGAnimatedPropertyBase* binding,
                                 PropertyIsAnimValType property_is_anim_val)
    : SVGPropertyTearOff<SVGPoint>(target, binding, property_is_anim_val) {}

SVGPointTearOff::SVGPointTearOff(SVGPoint* target, SVGElement* context_element)
    : SVGPropertyTearOff<SVGPoint>(target, context_element) {}

void SVGPointTearOff::setX(float f, ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  Target()->SetX(f);
  CommitChange();
}

void SVGPointTearOff::setY(float f, ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return;
  }
  Target()->SetY(f);
  CommitChange();
}

SVGPointTearOff* SVGPointTearOff::matrixTransform(SVGMatrixTearOff* matrix) {
  FloatPoint point = Target()->MatrixTransform(matrix->Value());
  return CreateDetached(point);
}

SVGPointTearOff* SVGPointTearOff::CreateDetached(const FloatPoint& point) {
  return MakeGarbageCollected<SVGPointTearOff>(
      MakeGarbageCollected<SVGPoint>(point), nullptr, kPropertyIsNotAnimVal);
}

}  // namespace blink
