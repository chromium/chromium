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

#include "third_party/blink/renderer/core/svg/properties/svg_property_tear_off.h"

#include "third_party/blink/renderer/core/svg/properties/svg_animated_property.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

SVGPropertyTearOffBase::SVGPropertyTearOffBase(
    SVGAnimatedPropertyBase* binding,
    PropertyIsAnimValType property_is_anim_val)
    : context_element_(binding ? binding->ContextElement() : nullptr),
      binding_(binding),
      property_is_anim_val_(property_is_anim_val) {}

SVGPropertyTearOffBase::SVGPropertyTearOffBase(SVGElement* context_element)
    : context_element_(context_element),
      binding_(nullptr),
      property_is_anim_val_(kPropertyIsNotAnimVal) {}

void SVGPropertyTearOffBase::Trace(Visitor* visitor) const {
  visitor->Trace(context_element_);
  visitor->Trace(binding_);
  ScriptWrappable::Trace(visitor);
}

void SVGPropertyTearOffBase::ThrowReadOnly(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(
      DOMExceptionCode::kNoModificationAllowedError,
      ExceptionMessages::ReadOnly());
}

void SVGPropertyTearOffBase::ThrowIndexSize(ExceptionState& exception_state,
                                            uint32_t index,
                                            uint32_t max_bound) {
  exception_state.ThrowDOMException(
      DOMExceptionCode::kIndexSizeError,
      ExceptionMessages::IndexExceedsMaximumBound("index", index, max_bound));
}

void SVGPropertyTearOffBase::Bind(SVGAnimatedPropertyBase* binding) {
  DCHECK(!IsImmutable());
  DCHECK(binding);
  DCHECK(binding->ContextElement());
  context_element_ = binding->ContextElement();
  binding_ = binding;
}

void SVGPropertyTearOffBase::CommitChange(SVGPropertyCommitReason reason) {
  // Immutable (or animVal) objects should never mutate, so this hook should
  // never be called in those cases.
  DCHECK(!IsImmutable());
  DCHECK(!IsAnimVal());
  if (!binding_)
    return;
  binding_->BaseValueChanged(
      reason == SVGPropertyCommitReason::kListCleared
          ? SVGAnimatedPropertyBase::BaseValueChangeType::kRemoved
          : SVGAnimatedPropertyBase::BaseValueChangeType::kUpdated);
}

void SVGPropertyTearOffBase::EnsureAnimValUpdated() {
  DCHECK(IsImmutable());
  DCHECK(binding_);
  binding_->EnsureAnimValUpdated();
}

}  // namespace blink
