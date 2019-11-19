// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_GESTURE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_GESTURE_MANAGER_H_

#include "base/macros.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/page/event_with_hit_test_results.h"

namespace blink {

class IntPoint;
class ScrollManager;
class SelectionController;
class PointerEventManager;
class MouseEventManager;

// This class takes care of gestures and delegating the action based on the
// gesture to the responsible class.
class CORE_EXPORT GestureManager final
    : public GarbageCollected<GestureManager> {
 public:
  GestureManager(LocalFrame&,
                 ScrollManager&,
                 MouseEventManager&,
                 PointerEventManager&,
                 SelectionController&);
  void Trace(blink::Visitor*);

  void Clear();

  HitTestRequest::HitTestRequestType GetHitTypeForGestureType(
      WebInputEvent::Type);
  WebInputEventResult HandleGestureEventInFrame(
      const GestureEventWithHitTestResults&);
  bool LongTapShouldInvokeContextMenu() const;

 private:
  WebInputEventResult HandleGestureShowPress();
  WebInputEventResult HandleGestureTapDown(
      const GestureEventWithHitTestResults&);
  WebInputEventResult HandleGestureTap(const GestureEventWithHitTestResults&);
  WebInputEventResult HandleGestureLongPress(
      const GestureEventWithHitTestResults&);
  WebInputEventResult HandleGestureLongTap(
      const GestureEventWithHitTestResults&);
  WebInputEventResult HandleGestureTwoFingerTap(
      const GestureEventWithHitTestResults&);

  WebInputEventResult SendContextMenuEventForGesture(
      const GestureEventWithHitTestResults&);
  // Shows the Unhandled Tap UI if needed.
  void ShowUnhandledTapUIIfNeeded(bool dom_tree_changed,
                                  bool style_changed,
                                  Node* tapped_node,
                                  Element* tapped_element,
                                  const IntPoint& tapped_position_in_viewport);

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared if needed in |GestureManager::clear()|.

  const Member<LocalFrame> frame_;

  Member<ScrollManager> scroll_manager_;
  Member<MouseEventManager> mouse_event_manager_;
  Member<PointerEventManager> pointer_event_manager_;

  // Set on GestureTapDown if the |pointerdown| event corresponding to the
  // triggering |touchstart| event was canceled. This suppresses mouse event
  // firing for the current gesture sequence (i.e. until next GestureTapDown).
  bool suppress_mouse_events_from_gestures_;

  bool long_tap_should_invoke_context_menu_;

  const Member<SelectionController> selection_controller_;

  DISALLOW_COPY_AND_ASSIGN(GestureManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INPUT_GESTURE_MANAGER_H_
