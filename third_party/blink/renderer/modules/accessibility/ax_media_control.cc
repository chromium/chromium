// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_media_control.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
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
    : AXLayoutObject(layout_object, ax_object_cache) {}

bool AccessibilityMediaControl::InternalSetAccessibilityFocusAction() {
  MediaControlElementsHelper::NotifyMediaControlAccessibleFocus(GetElement());
  return true;
}

bool AccessibilityMediaControl::InternalClearAccessibilityFocusAction() {
  MediaControlElementsHelper::NotifyMediaControlAccessibleBlur(GetElement());
  return true;
}

}  // namespace blink
