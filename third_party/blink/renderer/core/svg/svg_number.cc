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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/svg/svg_number.h"

#include "third_party/blink/renderer/core/svg/animation/smil_animation_effect_parameters.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"

namespace blink {

SVGNumber::SVGNumber(float value) : value_(value) {}

SVGNumber* SVGNumber::Clone() const {
  return MakeGarbageCollected<SVGNumber>(value_);
}

SVGPropertyBase* SVGNumber::CloneForAnimation(const String& value) const {
  auto* property = MakeGarbageCollected<SVGNumber>();
  property->SetValueAsString(value);
  return property;
}

String SVGNumber::ValueAsString() const {
  return String::Number(value_);
}

template <typename CharType>
SVGParsingError SVGNumber::Parse(const CharType* ptr, const CharType* end) {
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

  if (string.empty())
    return SVGParseStatus::kNoError;

  return WTF::VisitCharacters(string, [&](auto chars) {
    return Parse(chars.data(), chars.data() + chars.size());
  });
}

void SVGNumber::Add(const SVGPropertyBase* other, const SVGElement*) {
  SetValue(value_ + To<SVGNumber>(other)->Value());
}

void SVGNumber::CalculateAnimatedValue(
    const SMILAnimationEffectParameters& parameters,
    float percentage,
    unsigned repeat_count,
    const SVGPropertyBase* from,
    const SVGPropertyBase* to,
    const SVGPropertyBase* to_at_end_of_duration,
    const SVGElement*) {
  auto* from_number = To<SVGNumber>(from);
  auto* to_number = To<SVGNumber>(to);
  auto* to_at_end_of_duration_number = To<SVGNumber>(to_at_end_of_duration);

  float result = ComputeAnimatedNumber(parameters, percentage, repeat_count,
                                       from_number->Value(), to_number->Value(),
                                       to_at_end_of_duration_number->Value());
  if (parameters.is_additive)
    result += value_;

  value_ = result;
}

float SVGNumber::CalculateDistance(const SVGPropertyBase* other,
                                   const SVGElement*) const {
  return fabsf(value_ - To<SVGNumber>(other)->Value());
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

  if (string.empty())
    return SVGParseStatus::kExpectedNumberOrPercentage;

  float number = 0;
  SVGParsingError error = WTF::VisitCharacters(string, [&](auto chars) {
    const auto* start = chars.data();
    return ParseNumberOrPercentage(start, start + chars.size(), number);
  });
  if (error == SVGParseStatus::kNoError)
    value_ = number;
  return error;
}

SVGNumberAcceptPercentage::SVGNumberAcceptPercentage(float value)
    : SVGNumber(value) {}

}  // namespace blink
