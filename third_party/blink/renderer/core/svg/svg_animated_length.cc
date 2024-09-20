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

#include "third_party/blink/renderer/core/svg/svg_animated_length.h"

#include "third_party/blink/renderer/core/svg/svg_length.h"

namespace blink {

namespace {

bool RequireNonNegative(CSSPropertyID property_id) {
  // This should include more properties ('r', 'rx' and 'ry').
  return property_id == CSSPropertyID::kWidth ||
         property_id == CSSPropertyID::kHeight;
}

}  // namespace

SVGParsingError SVGAnimatedLength::AttributeChanged(const String& value) {
  SVGParsingError parse_status =
      SVGAnimatedProperty<SVGLength>::AttributeChanged(value);

  if (SVGLength::NegativeValuesForbiddenForAnimatedLengthAttribute(
          AttributeName())) {
    // TODO(crbug.com/982425): Pass |kValueRangeNonNegative| to property parser
    // to handle range checking on math functions correctly, and also to avoid
    // this ad hoc range checking.
    if (BaseValue()->IsNegativeNumericLiteral())
      parse_status = SVGParseStatus::kNegativeValue;
  }

  return parse_status;
}

const CSSValue* SVGAnimatedLength::CssValue() const {
  DCHECK(HasPresentationAttributeMapping());
  // SVG allows negative numbers for these attributes but CSS doesn't allow
  // negative <length> values for the corresponding CSS properties. So remove
  // negative values here.
  if (RequireNonNegative(CssPropertyId())) {
    // TODO(fs): This doesn't handle calc expressions. For that, we'd probably
    // need to rewrap the CSSMathExpressionNode with a kValueRangeNonNegative
    // range specification.
    if (CurrentValue()->IsNegativeNumericLiteral()) {
      return nullptr;
    }
  }
  return &CurrentValue()->AsCSSPrimitiveValue();
}

void SVGAnimatedLength::Trace(Visitor* visitor) const {
  SVGAnimatedProperty<SVGLength>::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
