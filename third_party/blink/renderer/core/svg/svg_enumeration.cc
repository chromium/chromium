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

#include "third_party/blink/renderer/core/svg/svg_enumeration.h"

#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"

namespace blink {

DEFINE_SVG_PROPERTY_TYPE_CASTS(SVGEnumerationBase);

SVGEnumerationBase::~SVGEnumerationBase() = default;

SVGPropertyBase* SVGEnumerationBase::CloneForAnimation(
    const String& value) const {
  SVGEnumerationBase* svg_enumeration = Clone();
  svg_enumeration->SetValueAsString(value);
  return svg_enumeration;
}

String SVGEnumerationBase::ValueAsString() const {
  if (const char* enum_name = map_.NameFromValue(value_))
    return String(enum_name);

  DCHECK_LT(value_, MaxInternalEnumValue());
  return g_empty_string;
}

void SVGEnumerationBase::SetValue(uint16_t value) {
  value_ = value;
  NotifyChange();
}

SVGParsingError SVGEnumerationBase::SetValueAsString(const String& string) {
  uint16_t value = map_.ValueFromName(string);
  if (value) {
    value_ = value;
    NotifyChange();
    return SVGParseStatus::kNoError;
  }
  NotifyChange();
  return SVGParseStatus::kExpectedEnumeration;
}

void SVGEnumerationBase::Add(SVGPropertyBase*, SVGElement*) {
  NOTREACHED();
}

void SVGEnumerationBase::CalculateAnimatedValue(
    const SVGAnimateElement& animation_element,
    float percentage,
    unsigned repeat_count,
    SVGPropertyBase* from,
    SVGPropertyBase* to,
    SVGPropertyBase*,
    SVGElement*) {
  NOTREACHED();
}

float SVGEnumerationBase::CalculateDistance(SVGPropertyBase*, SVGElement*) {
  // No paced animations for boolean.
  return -1;
}

uint16_t SVGEnumerationBase::MaxExposedEnumValue() const {
  return map_.MaxExposedValue();
}

uint16_t SVGEnumerationBase::MaxInternalEnumValue() const {
  return map_.ValueOfLast();
}

}  // namespace blink
