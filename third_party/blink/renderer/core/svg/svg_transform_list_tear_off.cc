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

#include "third_party/blink/renderer/core/svg/svg_transform_list_tear_off.h"

#include "third_party/blink/renderer/core/svg/svg_transform_tear_off.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SVGTransformListTearOff::SVGTransformListTearOff(
    SVGTransformList* target,
    SVGAnimatedPropertyBase* binding,
    PropertyIsAnimValType property_is_anim_val)
    : SVGListPropertyTearOffHelper<SVGTransformListTearOff, SVGTransformList>(
          target,
          binding,
          property_is_anim_val) {}

SVGTransformListTearOff::~SVGTransformListTearOff() = default;

SVGTransformTearOff* SVGTransformListTearOff::createSVGTransformFromMatrix(
    SVGMatrixTearOff* matrix) const {
  return MakeGarbageCollected<SVGTransformTearOff>(matrix);
}

SVGTransformTearOff* SVGTransformListTearOff::consolidate(
    ExceptionState& exception_state) {
  if (IsImmutable()) {
    ThrowReadOnly(exception_state);
    return nullptr;
  }
  SVGTransformList* transform_list = Target();
  if (transform_list->IsEmpty())
    return nullptr;
  auto* concatenated_transform =
      MakeGarbageCollected<SVGTransform>(transform_list->Concatenate());
  transform_list->Clear();
  transform_list->Append(concatenated_transform);
  return AttachedItemTearOff(concatenated_transform);
}

}  // namespace blink
