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

#include "base/notreached.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"

namespace blink {

SVGPropertyBase* SVGEnumeration::CloneForAnimation(const String& value) const {
  SVGEnumeration* svg_enumeration = Clone();
  svg_enumeration->SetValueAsString(value);
  return svg_enumeration;
}

String SVGEnumeration::ValueAsString() const {
  if (const char* enum_name = map_.NameFromValue(value_))
    return String(enum_name);

  DCHECK_LT(value_, MaxInternalEnumValue());
  return g_empty_string;
}

void SVGEnumeration::SetValue(uint16_t value) {
  value_ = value;
  NotifyChange();
}

SVGParsingError SVGEnumeration::SetValueAsString(const String& string) {
  uint16_t value = map_.ValueFromName(string);
  if (value) {
    SetValue(value);
    return SVGParseStatus::kNoError;
  }
  NotifyChange();
  return SVGParseStatus::kExpectedEnumeration;
}

uint16_t SVGEnumeration::MaxExposedEnumValue() const {
  return map_.MaxExposedValue();
}

uint16_t SVGEnumeration::MaxInternalEnumValue() const {
  return map_.ValueOfLast();
}

void SVGEnumeration::Add(const SVGPropertyBase*, const SVGElement*) {
  NOTREACHED_IN_MIGRATION();
}

void SVGEnumeration::CalculateAnimatedValue(
    const SMILAnimationEffectParameters&,
    float percentage,
    unsigned repeat_count,
    const SVGPropertyBase* from,
    const SVGPropertyBase* to,
    const SVGPropertyBase*,
    const SVGElement*) {
  NOTREACHED_IN_MIGRATION();
}

float SVGEnumeration::CalculateDistance(const SVGPropertyBase*,
                                        const SVGElement*) const {
  // No paced animations for enumerations.
  return -1;
}

}  // namespace blink
