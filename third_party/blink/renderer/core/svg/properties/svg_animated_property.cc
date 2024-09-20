/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/properties/svg_animated_property.h"

#include "third_party/blink/renderer/core/svg/svg_element.h"

namespace blink {

SVGAnimatedPropertyBase::SVGAnimatedPropertyBase(
    AnimatedPropertyType type,
    SVGElement* context_element,
    const QualifiedName& attribute_name,
    CSSPropertyID css_property_id,
    unsigned initial_value)
    : type_(type),
      // Cast to avoid warnings about unsafe bitfield truncations of the CSS
      // property enum. CSS properties that don't fit in this bitfield are never
      // used here. See static_assert in header.
      css_property_id_(static_cast<unsigned>(css_property_id)),
      initial_value_storage_(initial_value),
      content_attribute_state_(kNotSet),
      context_element_(context_element),
      attribute_name_(attribute_name) {
  DCHECK(context_element_);
  DCHECK(attribute_name_ != QualifiedName::Null());
  DCHECK_EQ(GetType(), type);
  DCHECK_EQ(CssPropertyId(), css_property_id);
  DCHECK_EQ(initial_value_storage_, initial_value);
}

SVGAnimatedPropertyBase::~SVGAnimatedPropertyBase() = default;

void SVGAnimatedPropertyBase::Trace(Visitor* visitor) const {
  visitor->Trace(context_element_);
}

bool SVGAnimatedPropertyBase::NeedsSynchronizeAttribute() const {
  // DOM attribute synchronization is only needed if a change has been made
  // through the JavaScript IDL attribute (via a tear-off or primitive). This
  // prevents unnecessary attribute creation on the target element.
  return content_attribute_state_ == kUnsynchronizedValue ||
         content_attribute_state_ == kUnsynchronizedRemoval;
}

void SVGAnimatedPropertyBase::SynchronizeAttribute() {
  AtomicString value(BaseValueBase().ValueAsString());
  context_element_->SetSynchronizedLazyAttribute(attribute_name_, value);
  DCHECK(NeedsSynchronizeAttribute());
  SetContentAttributeState(value.IsNull() ? kNotSet : kHasValue);
}

const CSSValue* SVGAnimatedPropertyBase::CssValue() const {
  NOTREACHED();
}

void SVGAnimatedPropertyBase::BaseValueChanged(
    BaseValueChangeType change_type) {
  DCHECK(context_element_);
  DCHECK(attribute_name_ != QualifiedName::Null());
  SetContentAttributeState(change_type == BaseValueChangeType::kRemoved
                               ? kUnsynchronizedRemoval
                               : kUnsynchronizedValue);
  context_element_->BaseValueChanged(*this);
}

void SVGAnimatedPropertyBase::EnsureAnimValUpdated() {
  DCHECK(context_element_);
  context_element_->EnsureAttributeAnimValUpdated();
}

bool SVGAnimatedPropertyBase::IsSpecified() const {
  return HasContentAttribute() || IsAnimating();
}

}  // namespace blink
