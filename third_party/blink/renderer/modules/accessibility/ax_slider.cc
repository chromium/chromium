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
    : AXNodeObject(layout_object, ax_object_cache) {}

ax::mojom::blink::Role AXSlider::NativeRoleIgnoringAria() const {
  return ax::mojom::blink::Role::kSlider;
}

AccessibilityOrientation AXSlider::Orientation() const {
  // Default to horizontal in the unknown case.
  if (!GetLayoutObject()) {
    return kAccessibilityOrientationHorizontal;
  }

  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style)
    return kAccessibilityOrientationHorizontal;

  // If CSS writing-mode is vertical, return kAccessibilityOrientationVertical.
  if (!style->IsHorizontalWritingMode()) {
    return kAccessibilityOrientationVertical;
  }

  // Else, look at the CSS appearance property for slider orientation.
  ControlPart style_appearance = style->EffectiveAppearance();
  switch (style_appearance) {
    case kSliderThumbHorizontalPart:
    case kSliderHorizontalPart:
    case kMediaSliderPart:
      return kAccessibilityOrientationHorizontal;

    case kSliderVerticalPart:
      return RuntimeEnabledFeatures::
                     NonStandardAppearanceValueSliderVerticalEnabled()
                 ? kAccessibilityOrientationVertical
                 : kAccessibilityOrientationHorizontal;
    case kSliderThumbVerticalPart:
    case kMediaVolumeSliderPart:
      return kAccessibilityOrientationVertical;

    default:
      return kAccessibilityOrientationHorizontal;
  }
}

bool AXSlider::OnNativeSetValueAction(const String& value) {
  HTMLInputElement* input = GetInputElement();

  if (input->Value() == value)
    return false;

  input->SetValue(value, TextFieldEventBehavior::kDispatchInputAndChangeEvent);

  // Fire change event manually, as SliderThumbElement::StopDragging does.
  input->DispatchFormControlChangeEvent();

  // Dispatching an event could result in changes to the document, like
  // this AXObject becoming detached.
  if (IsDetached())
    return false;

  // Ensure the AX node is updated.
  AXObjectCache().HandleValueChanged(GetNode());

  return true;
}

HTMLInputElement* AXSlider::GetInputElement() const {
  return To<HTMLInputElement>(GetNode());
}

}  // namespace blink
