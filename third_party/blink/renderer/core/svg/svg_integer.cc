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
#include "third_party/blink/renderer/core/svg/animation/smil_animation_effect_parameters.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

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

  if (string.empty())
    return SVGParseStatus::kNoError;

  bool valid = true;
  value_ = StripLeadingAndTrailingHTMLSpaces(string).ToIntStrict(&valid);
  // toIntStrict returns 0 if valid == false.
  return valid ? SVGParseStatus::kNoError : SVGParseStatus::kExpectedInteger;
}

void SVGInteger::Add(const SVGPropertyBase* other, const SVGElement*) {
  SetValue(value_ + To<SVGInteger>(other)->Value());
}

void SVGInteger::CalculateAnimatedValue(
    const SMILAnimationEffectParameters& parameters,
    float percentage,
    unsigned repeat_count,
    const SVGPropertyBase* from,
    const SVGPropertyBase* to,
    const SVGPropertyBase* to_at_end_of_duration,
    const SVGElement*) {
  auto* from_integer = To<SVGInteger>(from);
  auto* to_integer = To<SVGInteger>(to);
  auto* to_at_end_of_duration_integer = To<SVGInteger>(to_at_end_of_duration);

  float result = ComputeAnimatedNumber(
      parameters, percentage, repeat_count, from_integer->Value(),
      to_integer->Value(), to_at_end_of_duration_integer->Value());
  if (parameters.is_additive)
    result += value_;

  value_ = ClampTo<int>(roundf(result));
}

float SVGInteger::CalculateDistance(const SVGPropertyBase* other,
                                    const SVGElement*) const {
  return abs(value_ - To<SVGInteger>(other)->Value());
}

}  // namespace blink
