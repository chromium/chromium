// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_media_control.h"

#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"

namespace blink {

// static
AXObject* AccessibilityMediaControl::Create(
    LayoutObject* layout_object,
    AXObjectCacheImpl& ax_object_cache) {
  DCHECK(layout_object->GetNode());
  return MakeGarbageCollected<AccessibilityMediaControl>(layout_object,
                                                         ax_object_cache);
}

AccessibilityMediaControl::AccessibilityMediaControl(
    LayoutObject* layout_object,
    AXObjectCacheImpl& ax_object_cache)
    : AXNodeObject(layout_object, ax_object_cache) {}

bool AccessibilityMediaControl::InternalSetAccessibilityFocusAction() {
  MediaControlElementsHelper::NotifyMediaControlAccessibleFocus(GetElement());
  return true;
}

bool AccessibilityMediaControl::InternalClearAccessibilityFocusAction() {
  MediaControlElementsHelper::NotifyMediaControlAccessibleBlur(GetElement());
  return true;
}

bool AccessibilityMediaControl::OnNativeSetValueAction(const String& value) {
  // We should only execute this action on a kInputRange.
  auto* input = DynamicTo<HTMLInputElement>(GetNode());
  if (!input ||
      input->FormControlType() != mojom::FormControlType::kInputRange) {
    return AXNodeObject::OnNativeSetValueAction(value);
  }

  if (input->Value() == value) {
    return false;
  }

  input->SetValue(value, TextFieldEventBehavior::kDispatchInputAndChangeEvent);

  // Fire change event manually, as SliderThumbElement::StopDragging does.
  input->DispatchFormControlChangeEvent();

  // Dispatching an event could result in changes to the document, like
  // this AXObject becoming detached.
  if (IsDetached()) {
    return false;
  }

  AXObjectCache().HandleValueChanged(GetNode());

  return true;
}

}  // namespace blink
