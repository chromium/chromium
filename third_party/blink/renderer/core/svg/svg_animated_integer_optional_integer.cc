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

#include "third_party/blink/renderer/core/svg/svg_animated_integer_optional_integer.h"

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SVGAnimatedIntegerOptionalInteger::SVGAnimatedIntegerOptionalInteger(
    SVGElement* context_element,
    const QualifiedName& attribute_name,
    int initial_value)
    : SVGAnimatedPropertyCommon<SVGIntegerOptionalInteger>(
          context_element,
          attribute_name,
          MakeGarbageCollected<SVGIntegerOptionalInteger>(
              MakeGarbageCollected<SVGInteger>(initial_value),
              MakeGarbageCollected<SVGInteger>(initial_value)),
          CSSPropertyID::kInvalid,
          initial_value),
      first_integer_(MakeGarbageCollected<SVGAnimatedInteger>(
          context_element,
          attribute_name,
          BaseValue()->FirstInteger())),
      second_integer_(MakeGarbageCollected<SVGAnimatedInteger>(
          context_element,
          attribute_name,
          BaseValue()->SecondInteger())) {
  first_integer_->SetParentOptionalInteger(this);
  second_integer_->SetParentOptionalInteger(this);
}

void SVGAnimatedIntegerOptionalInteger::Trace(Visitor* visitor) const {
  visitor->Trace(first_integer_);
  visitor->Trace(second_integer_);
  SVGAnimatedPropertyCommon<SVGIntegerOptionalInteger>::Trace(visitor);
}

void SVGAnimatedIntegerOptionalInteger::SetAnimatedValue(
    SVGPropertyBase* value) {
  SVGAnimatedPropertyCommon<SVGIntegerOptionalInteger>::SetAnimatedValue(value);
  first_integer_->SetAnimatedValue(CurrentValue()->FirstInteger());
  second_integer_->SetAnimatedValue(CurrentValue()->SecondInteger());
}

bool SVGAnimatedIntegerOptionalInteger::NeedsSynchronizeAttribute() const {
  return first_integer_->NeedsSynchronizeAttribute() ||
         second_integer_->NeedsSynchronizeAttribute();
}

}  // namespace blink
