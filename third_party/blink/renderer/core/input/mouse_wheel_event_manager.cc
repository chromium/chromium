// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/mouse_wheel_event_manager.h"

#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/event_handling_util.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace blink {

namespace {

gfx::Vector2dF ResolveMouseWheelPercentToWheelDelta(
    const WebMouseWheelEvent& event) {
  DCHECK(event.delta_units == ui::ScrollGranularity::kScrollByPercentage);
  // TODO (dlibby): OS scroll settings need to be factored into this.
  // Note that this value is negative because we're converting from wheel
  // ticks to wheel delta pixel. Wheel ticks are negative for scrolling down,
  // but the delta must be positive.
  constexpr float percent_mouse_wheel_ticks_multiplier = -100.f;
  return gfx::Vector2dF(
      event.wheel_ticks_x * percent_mouse_wheel_ticks_multiplier,
      event.wheel_ticks_y * percent_mouse_wheel_ticks_multiplier);
}

}  // namespace

MouseWheelEventManager::MouseWheelEventManager(LocalFrame& frame,
                                               ScrollManager& scroll_manager)
    : frame_(frame), wheel_target_(nullptr), scroll_manager_(scroll_manager) {}

void MouseWheelEventManager::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(wheel_target_);
  visitor->Trace(scroll_manager_);
}

void MouseWheelEventManager::Clear() {
  wheel_target_ = nullptr;
}

WebInputEventResult MouseWheelEventManager::HandleWheelEvent(
    const WebMouseWheelEvent& event) {
  Document* doc = frame_->GetDocument();
  if (!doc || !doc->GetLayoutView())
    return WebInputEventResult::kNotHandled;

  LocalFrameView* view = frame_->View();
  if (!view)
    return WebInputEventResult::kNotHandled;

  const int kWheelEventPhaseEndedEventMask =
      WebMouseWheelEvent::kPhaseEnded | WebMouseWheelEvent::kPhaseCancelled;
  const int kWheelEventPhaseNoEventMask =
      kWheelEventPhaseEndedEventMask | WebMouseWheelEvent::kPhaseMayBegin;

  if ((event.phase & kWheelEventPhaseEndedEventMask) ||
      (event.momentum_phase & kWheelEventPhaseEndedEventMask)) {
    wheel_target_ = nullptr;
  }

  if ((event.phase & kWheelEventPhaseNoEventMask) ||
      (event.momentum_phase & kWheelEventPhaseNoEventMask)) {
    return WebInputEventResult::kNotHandled;
  }

  // Synthetic wheel events generated from GestureDoubleTap are phaseless.
  // Wheel events generated from plugin and tests may not have phase info.
  bool has_phase_info = event.phase != WebMouseWheelEvent::kPhaseNone ||
                        event.momentum_phase != WebMouseWheelEvent::kPhaseNone;

  Element* pointer_locked_element =
      PointerLockController::GetPointerLockedElement(frame_);
  if (pointer_locked_element) {
    wheel_target_ = pointer_locked_element;
  } else {
    // Find and save the wheel_target_, this target will be used for the rest
    // of the current scrolling sequence. In the absence of phase info, send the
    // event to the target under the cursor.
    if (event.phase == WebMouseWheelEvent::kPhaseBegan || !wheel_target_ ||
        !has_phase_info) {
      wheel_target_ = FindTargetNode(event, doc, view);
    }
  }

  LocalFrame* subframe =
      event_handling_util::SubframeForTargetNode(wheel_target_.Get());
  if (subframe) {
    WebInputEventResult result =
        subframe->GetEventHandler().HandleWheelEvent(event);
    return result;
  }

  if (wheel_target_) {
    WheelEvent* dom_event =
        (event.delta_units == ui::ScrollGranularity::kScrollByPercentage)
            ? WheelEvent::Create(event,
                                 ResolveMouseWheelPercentToWheelDelta(event),
                                 *wheel_target_->GetDocument().domWindow())
            : WheelEvent::Create(event,
                                 *wheel_target_->GetDocument().domWindow());

    // The event handler might remove |wheel_target_| from DOM so we should get
    // this value now (see https://crbug.com/857013).
    bool should_enforce_vertical_scroll =
        wheel_target_->GetDocument().IsVerticalScrollEnforced();
    DispatchEventResult dom_event_result =
        wheel_target_->DispatchEvent(*dom_event);
    if (dom_event_result != DispatchEventResult::kNotCanceled) {
      // Reset the target if the dom event is cancelled to make sure that new
      // targeting happens for the next wheel event.
      wheel_target_ = nullptr;

      bool is_vertical = dom_event->NativeEvent().event_action ==
                         WebMouseWheelEvent::EventAction::kScrollVertical;
      // TODO(ekaramad): If the only wheel handlers on the page are from such
      // disabled frames we should simply start scrolling on CC and the events
      // must get here as passive (https://crbug.com/853059).
      // Overwriting the dispatch results ensures that vertical scroll cannot be
      // blocked by disabled frames.
      return (should_enforce_vertical_scroll && is_vertical)
                 ? WebInputEventResult::kNotHandled
                 : event_handling_util::ToWebInputEventResult(dom_event_result);
    }
  }

  return WebInputEventResult::kNotHandled;
}

void MouseWheelEventManager::ElementRemoved(Node* target) {
  if (wheel_target_ == target)
    wheel_target_ = nullptr;
}

Node* MouseWheelEventManager::FindTargetNode(const WebMouseWheelEvent& event,
                                             const Document* doc,
                                             const LocalFrameView* view) {
  DCHECK(doc && doc->GetLayoutView() && view);
  PhysicalOffset v_point(view->ConvertFromRootFrame(
      gfx::ToFlooredPoint(event.PositionInRootFrame())));

  HitTestRequest request(HitTestRequest::kReadOnly);
  HitTestLocation location(v_point);
  HitTestResult result(request, location);
  doc->GetLayoutView()->HitTest(location, result);

  Node* node = result.InnerNode();
  // Wheel events should not dispatch to text nodes.
  if (node && node->IsTextNode())
    node = FlatTreeTraversal::Parent(*node);

  // If we're over the frame scrollbar, scroll the document.
  if (!node && result.GetScrollbar())
    node = doc->documentElement();

  return node;
}

}  // namespace blink
