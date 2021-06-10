/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/css_crossfade_value.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

CSSCrossfadeValue::CSSCrossfadeValue(CSSValue* from_value,
                                     CSSValue* to_value,
                                     CSSPrimitiveValue* percentage_value)
    : CSSImageGeneratorValue(kCrossfadeClass),
      from_value_(from_value),
      to_value_(to_value),
      percentage_value_(percentage_value) {}

CSSCrossfadeValue::~CSSCrossfadeValue() = default;

String CSSCrossfadeValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("-webkit-cross-fade(");
  result.Append(from_value_->CssText());
  result.Append(", ");
  result.Append(to_value_->CssText());
  result.Append(", ");
  result.Append(percentage_value_->CssText());
  result.Append(')');
  return result.ToString();
}

bool CSSCrossfadeValue::HasFailedOrCanceledSubresources() const {
  return from_value_->HasFailedOrCanceledSubresources() ||
         to_value_->HasFailedOrCanceledSubresources();
}

bool CSSCrossfadeValue::Equals(const CSSCrossfadeValue& other) const {
  return DataEquivalent(from_value_, other.from_value_) &&
         DataEquivalent(to_value_, other.to_value_) &&
         DataEquivalent(percentage_value_, other.percentage_value_);
}

void CSSCrossfadeValue::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(from_value_);
  visitor->Trace(to_value_);
  visitor->Trace(percentage_value_);
  CSSImageGeneratorValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink
