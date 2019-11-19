/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/svg/svg_animated_number_optional_number.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

SVGAnimatedNumberOptionalNumber::SVGAnimatedNumberOptionalNumber(
    SVGElement* context_element,
    const QualifiedName& attribute_name,
    float initial_value)
    : SVGAnimatedPropertyCommon<SVGNumberOptionalNumber>(
          context_element,
          attribute_name,
          MakeGarbageCollected<SVGNumberOptionalNumber>(
              MakeGarbageCollected<SVGNumber>(initial_value),
              MakeGarbageCollected<SVGNumber>(initial_value)),
          CSSPropertyID::kInvalid,
          static_cast<unsigned>(initial_value)),
      first_number_(
          MakeGarbageCollected<SVGAnimatedNumber>(context_element,
                                                  attribute_name,
                                                  BaseValue()->FirstNumber())),
      second_number_(MakeGarbageCollected<SVGAnimatedNumber>(
          context_element,
          attribute_name,
          BaseValue()->SecondNumber())) {
  first_number_->SetParentOptionalNumber(this);
  second_number_->SetParentOptionalNumber(this);
}

void SVGAnimatedNumberOptionalNumber::Trace(blink::Visitor* visitor) {
  visitor->Trace(first_number_);
  visitor->Trace(second_number_);
  SVGAnimatedPropertyCommon<SVGNumberOptionalNumber>::Trace(visitor);
}

void SVGAnimatedNumberOptionalNumber::SetAnimatedValue(SVGPropertyBase* value) {
  SVGAnimatedPropertyCommon<SVGNumberOptionalNumber>::SetAnimatedValue(value);
  first_number_->SetAnimatedValue(CurrentValue()->FirstNumber());
  second_number_->SetAnimatedValue(CurrentValue()->SecondNumber());
}

void SVGAnimatedNumberOptionalNumber::AnimationEnded() {
  SVGAnimatedPropertyCommon<SVGNumberOptionalNumber>::AnimationEnded();
  first_number_->AnimationEnded();
  second_number_->AnimationEnded();
}

bool SVGAnimatedNumberOptionalNumber::NeedsSynchronizeAttribute() const {
  return first_number_->NeedsSynchronizeAttribute() ||
         second_number_->NeedsSynchronizeAttribute();
}

}  // namespace blink
