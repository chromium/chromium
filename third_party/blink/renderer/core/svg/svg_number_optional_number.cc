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

#include "third_party/blink/renderer/core/svg/svg_number_optional_number.h"

#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

SVGNumberOptionalNumber::SVGNumberOptionalNumber(SVGNumber* first_number,
                                                 SVGNumber* second_number)
    : first_number_(first_number), second_number_(second_number) {}

void SVGNumberOptionalNumber::Trace(Visitor* visitor) const {
  visitor->Trace(first_number_);
  visitor->Trace(second_number_);
  SVGPropertyBase::Trace(visitor);
}

SVGNumberOptionalNumber* SVGNumberOptionalNumber::Clone() const {
  return MakeGarbageCollected<SVGNumberOptionalNumber>(first_number_->Clone(),
                                                       second_number_->Clone());
}

SVGPropertyBase* SVGNumberOptionalNumber::CloneForAnimation(
    const String& value) const {
  float x, y;
  if (!ParseNumberOptionalNumber(value, x, y)) {
    x = y = 0;
  }

  return MakeGarbageCollected<SVGNumberOptionalNumber>(
      MakeGarbageCollected<SVGNumber>(x), MakeGarbageCollected<SVGNumber>(y));
}

String SVGNumberOptionalNumber::ValueAsString() const {
  if (first_number_->Value() == second_number_->Value()) {
    return String::Number(first_number_->Value());
  }

  return String::Number(first_number_->Value()) + " " +
         String::Number(second_number_->Value());
}

SVGParsingError SVGNumberOptionalNumber::SetValueAsString(const String& value) {
  float x, y;
  SVGParsingError parse_status;
  if (!ParseNumberOptionalNumber(value, x, y)) {
    parse_status = SVGParseStatus::kExpectedNumber;
    x = y = 0;
  }

  first_number_->SetValue(x);
  second_number_->SetValue(y);
  return parse_status;
}

void SVGNumberOptionalNumber::SetInitial(unsigned value) {
  // Propagate the value to the split representation.
  first_number_->SetInitial(value);
  second_number_->SetInitial(value);
}

void SVGNumberOptionalNumber::Add(const SVGPropertyBase* other,
                                  const SVGElement* context_element) {
  auto* other_number_optional_number = To<SVGNumberOptionalNumber>(other);
  first_number_->Add(other_number_optional_number->FirstNumber(),
                     context_element);
  second_number_->Add(other_number_optional_number->SecondNumber(),
                      context_element);
}

void SVGNumberOptionalNumber::CalculateAnimatedValue(
    const SMILAnimationEffectParameters& parameters,
    float percentage,
    unsigned repeat_count,
    const SVGPropertyBase* from,
    const SVGPropertyBase* to,
    const SVGPropertyBase* to_at_end_of_duration,
    const SVGElement* context_element) {
  auto* from_number = To<SVGNumberOptionalNumber>(from);
  auto* to_number = To<SVGNumberOptionalNumber>(to);
  auto* to_at_end_of_duration_number =
      To<SVGNumberOptionalNumber>(to_at_end_of_duration);

  first_number_->CalculateAnimatedValue(
      parameters, percentage, repeat_count, from_number->FirstNumber(),
      to_number->FirstNumber(), to_at_end_of_duration_number->FirstNumber(),
      context_element);
  second_number_->CalculateAnimatedValue(
      parameters, percentage, repeat_count, from_number->SecondNumber(),
      to_number->SecondNumber(), to_at_end_of_duration_number->SecondNumber(),
      context_element);
}

float SVGNumberOptionalNumber::CalculateDistance(const SVGPropertyBase* other,
                                                 const SVGElement*) const {
  // FIXME: Distance calculation is not possible for SVGNumberOptionalNumber
  // right now. We need the distance for every single value.
  return -1;
}

}  // namespace blink
