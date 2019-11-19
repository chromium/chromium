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

#include "third_party/blink/renderer/core/svg/svg_number.h"

#include "third_party/blink/renderer/core/svg/svg_animate_element.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

SVGNumber::SVGNumber(float value) : value_(value) {}

SVGNumber* SVGNumber::Clone() const {
  return MakeGarbageCollected<SVGNumber>(value_);
}

String SVGNumber::ValueAsString() const {
  return String::Number(value_);
}

template <typename CharType>
SVGParsingError SVGNumber::Parse(const CharType*& ptr, const CharType* end) {
  float value = 0;
  const CharType* start = ptr;
  if (!ParseNumber(ptr, end, value, kAllowLeadingAndTrailingWhitespace))
    return SVGParsingError(SVGParseStatus::kExpectedNumber, ptr - start);
  if (ptr != end)
    return SVGParsingError(SVGParseStatus::kTrailingGarbage, ptr - start);
  value_ = value;
  return SVGParseStatus::kNoError;
}

SVGParsingError SVGNumber::SetValueAsString(const String& string) {
  value_ = 0;

  if (string.IsEmpty())
    return SVGParseStatus::kNoError;

  if (string.Is8Bit()) {
    const LChar* ptr = string.Characters8();
    const LChar* end = ptr + string.length();
    return Parse(ptr, end);
  }
  const UChar* ptr = string.Characters16();
  const UChar* end = ptr + string.length();
  return Parse(ptr, end);
}

void SVGNumber::Add(SVGPropertyBase* other, SVGElement*) {
  SetValue(value_ + ToSVGNumber(other)->Value());
}

void SVGNumber::CalculateAnimatedValue(
    const SVGAnimateElement& animation_element,
    float percentage,
    unsigned repeat_count,
    SVGPropertyBase* from,
    SVGPropertyBase* to,
    SVGPropertyBase* to_at_end_of_duration,
    SVGElement*) {
  SVGNumber* from_number = ToSVGNumber(from);
  SVGNumber* to_number = ToSVGNumber(to);
  SVGNumber* to_at_end_of_duration_number = ToSVGNumber(to_at_end_of_duration);

  animation_element.AnimateAdditiveNumber(
      percentage, repeat_count, from_number->Value(), to_number->Value(),
      to_at_end_of_duration_number->Value(), value_);
}

float SVGNumber::CalculateDistance(SVGPropertyBase* other, SVGElement*) {
  return fabsf(value_ - ToSVGNumber(other)->Value());
}

SVGNumber* SVGNumberAcceptPercentage::Clone() const {
  return MakeGarbageCollected<SVGNumberAcceptPercentage>(value_);
}

template <typename CharType>
static SVGParsingError ParseNumberOrPercentage(const CharType*& ptr,
                                               const CharType* end,
                                               float& number) {
  const CharType* start = ptr;
  if (!ParseNumber(ptr, end, number, kAllowLeadingWhitespace))
    return SVGParsingError(SVGParseStatus::kExpectedNumberOrPercentage,
                           ptr - start);
  if (ptr < end && *ptr == '%') {
    number /= 100;
    ptr++;
  }
  if (SkipOptionalSVGSpaces(ptr, end))
    return SVGParsingError(SVGParseStatus::kTrailingGarbage, ptr - start);
  return SVGParseStatus::kNoError;
}

SVGParsingError SVGNumberAcceptPercentage::SetValueAsString(
    const String& string) {
  value_ = 0;

  if (string.IsEmpty())
    return SVGParseStatus::kExpectedNumberOrPercentage;

  float number = 0;
  SVGParsingError error;
  if (string.Is8Bit()) {
    const LChar* ptr = string.Characters8();
    const LChar* end = ptr + string.length();
    error = ParseNumberOrPercentage(ptr, end, number);
  } else {
    const UChar* ptr = string.Characters16();
    const UChar* end = ptr + string.length();
    error = ParseNumberOrPercentage(ptr, end, number);
  }
  if (error == SVGParseStatus::kNoError)
    value_ = number;
  return error;
}

SVGNumberAcceptPercentage::SVGNumberAcceptPercentage(float value)
    : SVGNumber(value) {}

}  // namespace blink
