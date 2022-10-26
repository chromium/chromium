// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/touch_action_util.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {
namespace touch_action_util {

TouchAction ComputeEffectiveTouchAction(const Node& node) {
  if (node.GetComputedStyle())
    return node.GetComputedStyle()->EffectiveTouchAction();

  return TouchAction::kAuto;
}

TouchAction EffectiveTouchActionAtPointerDown(const WebPointerEvent& event,
                                              const Node* pointerdown_node) {
  DCHECK(event.GetType() == WebInputEvent::Type::kPointerDown);
  return EffectiveTouchActionAtPointer(event, pointerdown_node);
}

TouchAction EffectiveTouchActionAtPointer(const WebPointerEvent& event,
                                          const Node* node_at_pointer) {
  DCHECK(node_at_pointer);

  TouchAction effective_touch_action =
      ComputeEffectiveTouchAction(*node_at_pointer);

  if ((effective_touch_action & TouchAction::kPanX) != TouchAction::kNone) {
    // Effective touch action is computed during style before we know whether
    // any ancestor supports horizontal scrolling, so we need to check it here.
    if (LayoutBox::HasHorizontallyScrollableAncestor(
            node_at_pointer->GetLayoutObject())) {
      // If the node or its parent is horizontal scrollable, we need to disable
      // swipe to move cursor.
      effective_touch_action |= TouchAction::kInternalPanXScrolls;
    }
  }

  // Re-enable not writable bit if effective touch action does not allow panning
  // in all directions as writing can be started in any direction. Also, enable
  // this bit if pointer type is not stylus.
  if ((effective_touch_action & TouchAction::kPan) != TouchAction::kNone &&
      ((event.pointer_type != WebPointerProperties::PointerType::kPen &&
        event.pointer_type != WebPointerProperties::PointerType::kEraser) ||
       (effective_touch_action & TouchAction::kPan) != TouchAction::kPan)) {
    effective_touch_action |= TouchAction::kInternalNotWritable;
  }

  return effective_touch_action;
}

}  // namespace touch_action_util
}  // namespace blink
