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

#include "third_party/blink/renderer/core/svg/svg_point.h"

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

SVGPoint::SVGPoint() = default;

SVGPoint::SVGPoint(const gfx::PointF& point) : value_(point) {}

SVGPoint* SVGPoint::Clone() const {
  return MakeGarbageCollected<SVGPoint>(value_);
}

SVGPropertyBase* SVGPoint::CloneForAnimation(const String& value) const {
  // SVGPoint is not animated by itself.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

String SVGPoint::ValueAsString() const {
  StringBuilder builder;
  builder.AppendNumber(X());
  builder.Append(' ');
  builder.AppendNumber(Y());
  return builder.ToString();
}

void SVGPoint::Add(const SVGPropertyBase* other, const SVGElement*) {
  // SVGPoint is not animated by itself.
  NOTREACHED_IN_MIGRATION();
}

void SVGPoint::CalculateAnimatedValue(
    const SMILAnimationEffectParameters&,
    float percentage,
    unsigned repeat_count,
    const SVGPropertyBase* from_value,
    const SVGPropertyBase* to_value,
    const SVGPropertyBase* to_at_end_of_duration_value,
    const SVGElement*) {
  // SVGPoint is not animated by itself.
  NOTREACHED_IN_MIGRATION();
}

float SVGPoint::CalculateDistance(const SVGPropertyBase* to,
                                  const SVGElement* context_element) const {
  // SVGPoint is not animated by itself.
  NOTREACHED_IN_MIGRATION();
  return 0.0f;
}

}  // namespace blink
