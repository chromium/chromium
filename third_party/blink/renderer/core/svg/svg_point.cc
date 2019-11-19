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

#include "third_party/blink/renderer/core/svg/svg_animate_element.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

SVGPoint::SVGPoint() = default;

SVGPoint::SVGPoint(const FloatPoint& point) : value_(point) {}

SVGPoint* SVGPoint::Clone() const {
  return MakeGarbageCollected<SVGPoint>(value_);
}

template <typename CharType>
SVGParsingError SVGPoint::Parse(const CharType*& ptr, const CharType* end) {
  float x = 0;
  float y = 0;
  if (!ParseNumber(ptr, end, x) ||
      !ParseNumber(ptr, end, y, kDisallowWhitespace))
    return SVGParseStatus::kExpectedNumber;

  if (SkipOptionalSVGSpaces(ptr, end)) {
    // Nothing should come after the second number.
    return SVGParseStatus::kTrailingGarbage;
  }

  value_ = FloatPoint(x, y);
  return SVGParseStatus::kNoError;
}

FloatPoint SVGPoint::MatrixTransform(const AffineTransform& transform) const {
  double new_x, new_y;
  transform.Map(static_cast<double>(X()), static_cast<double>(Y()), new_x,
                new_y);
  return FloatPoint::NarrowPrecision(new_x, new_y);
}

SVGParsingError SVGPoint::SetValueAsString(const String& string) {
  if (string.IsEmpty()) {
    value_ = FloatPoint(0.0f, 0.0f);
    return SVGParseStatus::kNoError;
  }

  if (string.Is8Bit()) {
    const LChar* ptr = string.Characters8();
    const LChar* end = ptr + string.length();
    return Parse(ptr, end);
  }
  const UChar* ptr = string.Characters16();
  const UChar* end = ptr + string.length();
  return Parse(ptr, end);
}

String SVGPoint::ValueAsString() const {
  StringBuilder builder;
  builder.AppendNumber(X());
  builder.Append(' ');
  builder.AppendNumber(Y());
  return builder.ToString();
}

void SVGPoint::Add(SVGPropertyBase* other, SVGElement*) {
  // SVGPoint is not animated by itself
  NOTREACHED();
}

void SVGPoint::CalculateAnimatedValue(
    const SVGAnimateElement& animation_element,
    float percentage,
    unsigned repeat_count,
    SVGPropertyBase* from_value,
    SVGPropertyBase* to_value,
    SVGPropertyBase* to_at_end_of_duration_value,
    SVGElement*) {
  // SVGPoint is not animated by itself
  NOTREACHED();
}

float SVGPoint::CalculateDistance(SVGPropertyBase* to,
                                  SVGElement* context_element) {
  // SVGPoint is not animated by itself
  NOTREACHED();
  return 0.0f;
}

}  // namespace blink
