/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/accessibility/ax_slider.h"

#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"

namespace blink {

AXSlider::AXSlider(LayoutObject* layout_object,
                   AXObjectCacheImpl& ax_object_cache)
    : AXLayoutObject(layout_object, ax_object_cache) {}

ax::mojom::Role AXSlider::DetermineAccessibilityRole() {
  native_role_ = ax::mojom::blink::Role::kSlider;

  if ((aria_role_ = DetermineAriaRoleAttribute()) != ax::mojom::Role::kUnknown)
    return aria_role_;

  return native_role_;
}

AccessibilityOrientation AXSlider::Orientation() const {
  // Default to horizontal in the unknown case.
  if (!layout_object_)
    return kAccessibilityOrientationHorizontal;

  const ComputedStyle* style = layout_object_->Style();
  if (!style)
    return kAccessibilityOrientationHorizontal;

  ControlPart style_appearance = style->EffectiveAppearance();
  switch (style_appearance) {
    case kSliderThumbHorizontalPart:
    case kSliderHorizontalPart:
    case kMediaSliderPart:
      return kAccessibilityOrientationHorizontal;

    case kSliderThumbVerticalPart:
    case kSliderVerticalPart:
    case kMediaVolumeSliderPart:
      return kAccessibilityOrientationVertical;

    default:
      return kAccessibilityOrientationHorizontal;
  }
}

void AXSlider::AddChildren() {
  DCHECK(!IsDetached());
  DCHECK(!have_children_);

  have_children_ = true;

  AXObjectCacheImpl& cache = AXObjectCache();

  AXObject* thumb = cache.Create(ax::mojom::blink::Role::kSliderThumb, this);

  // Before actually adding the value indicator to the hierarchy,
  // allow the platform to make a final decision about it.
  if (!thumb->AccessibilityIsIncludedInTree())
    cache.Remove(thumb->AXObjectID());
  else
    children_.push_back(thumb);
}

AXObject* AXSlider::ElementAccessibilityHitTest(const IntPoint& point) const {
  if (children_.size()) {
    DCHECK(children_.size() == 1);
    if (children_[0]->GetBoundsInFrameCoordinates().Contains(point))
      return children_[0].Get();
  }

  return AXObjectCache().GetOrCreate(layout_object_);
}

bool AXSlider::OnNativeSetValueAction(const String& value) {
  HTMLInputElement* input = GetInputElement();

  if (input->value() == value)
    return false;

  input->setValue(value, TextFieldEventBehavior::kDispatchInputAndChangeEvent);

  // Fire change event manually, as SliderThumbElement::StopDragging does.
  input->DispatchFormControlChangeEvent();

  // Dispatching an event could result in changes to the document, like
  // this AXObject becoming detached.
  if (IsDetached())
    return false;

  // Ensure the AX node is updated.
  AXObjectCache().MarkAXObjectDirty(this, false);

  return true;
}

HTMLInputElement* AXSlider::GetInputElement() const {
  return To<HTMLInputElement>(layout_object_->GetNode());
}

AXSliderThumb::AXSliderThumb(AXObjectCacheImpl& ax_object_cache)
    : AXMockObject(ax_object_cache) {}

LayoutObject* AXSliderThumb::LayoutObjectForRelativeBounds() const {
  if (!parent_)
    return nullptr;

  LayoutObject* slider_layout_object = parent_->GetLayoutObject();
  if (!slider_layout_object)
    return nullptr;
  Element* thumb_element =
      To<Element>(slider_layout_object->GetNode())
          ->UserAgentShadowRoot()
          ->getElementById(shadow_element_names::SliderThumb());
  DCHECK(thumb_element);
  return thumb_element->GetLayoutObject();
}

bool AXSliderThumb::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  return AccessibilityIsIgnoredByDefault(ignored_reasons);
}

}  // namespace blink
