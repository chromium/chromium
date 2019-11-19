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

#include "third_party/blink/renderer/core/svg/svg_integer.h"

#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/svg/svg_animate_element.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

SVGInteger::SVGInteger(int value) : value_(value) {}

SVGInteger* SVGInteger::Clone() const {
  return MakeGarbageCollected<SVGInteger>(value_);
}

String SVGInteger::ValueAsString() const {
  return String::Number(value_);
}

SVGParsingError SVGInteger::SetValueAsString(const String& string) {
  value_ = 0;

  if (string.IsEmpty())
    return SVGParseStatus::kNoError;

  bool valid = true;
  value_ = StripLeadingAndTrailingHTMLSpaces(string).ToIntStrict(&valid);
  // toIntStrict returns 0 if valid == false.
  return valid ? SVGParseStatus::kNoError : SVGParseStatus::kExpectedInteger;
}

void SVGInteger::Add(SVGPropertyBase* other, SVGElement*) {
  SetValue(value_ + ToSVGInteger(other)->Value());
}

void SVGInteger::CalculateAnimatedValue(
    const SVGAnimateElement& animation_element,
    float percentage,
    unsigned repeat_count,
    SVGPropertyBase* from,
    SVGPropertyBase* to,
    SVGPropertyBase* to_at_end_of_duration,
    SVGElement*) {
  SVGInteger* from_integer = ToSVGInteger(from);
  SVGInteger* to_integer = ToSVGInteger(to);
  SVGInteger* to_at_end_of_duration_integer =
      ToSVGInteger(to_at_end_of_duration);

  float animated_float = value_;
  animation_element.AnimateAdditiveNumber(
      percentage, repeat_count, from_integer->Value(), to_integer->Value(),
      to_at_end_of_duration_integer->Value(), animated_float);
  value_ = clampTo<int>(roundf(animated_float));
}

float SVGInteger::CalculateDistance(SVGPropertyBase* other, SVGElement*) {
  return abs(value_ - ToSVGInteger(other)->Value());
}

}  // namespace blink
