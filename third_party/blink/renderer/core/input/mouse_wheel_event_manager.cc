// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/mouse_wheel_event_manager.h"

#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
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
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace blink {

MouseWheelEventManager::MouseWheelEventManager(LocalFrame& frame,
                                               ScrollManager& scroll_manager)
    : frame_(frame), wheel_target_(nullptr), scroll_manager_(scroll_manager) {}

void MouseWheelEventManager::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(wheel_target_);
  visitor->Trace(scroll_manager_);
  visitor->Trace(fade_out_deferred_scrollables_);
}

void MouseWheelEventManager::Clear() {
  UpdateWheelTarget(/*wheel_target=*/nullptr);
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

  if ((event.phase & kWheelEventPhaseEndedEventMask) ||
      (event.momentum_phase & kWheelEventPhaseEndedEventMask)) {
    UpdateWheelTarget(/*wheel_target=*/nullptr);
    return WebInputEventResult::kNotHandled;
  }

  if (Element* pointer_locked_element =
          PointerLockController::GetPointerLockedElement(frame_)) {
    UpdateWheelTarget(pointer_locked_element);
  } else {
    // Synthetic wheel events generated from GestureDoubleTap are phaseless.
    // Wheel events generated from plugin and tests may not have phase info.
    const bool has_phase_info =
        event.phase != WebMouseWheelEvent::kPhaseNone ||
        event.momentum_phase != WebMouseWheelEvent::kPhaseNone;

    const int kWheelEventPhaseStartedEventMask =
        WebMouseWheelEvent::kPhaseBegan | WebMouseWheelEvent::kPhaseMayBegin;
    // Find and save the wheel_target_, this target will be used for the rest
    // of the current scrolling sequence. In the absence of phase info, send the
    // event to the target under the cursor.
    if (event.phase & kWheelEventPhaseStartedEventMask || !wheel_target_ ||
        !has_phase_info) {
      UpdateWheelTarget(FindTargetNode(event, doc, view));
    }
  }

  LocalFrame* subframe =
      event_handling_util::SubframeForTargetNode(wheel_target_.Get());
  if (subframe) {
    return subframe->GetEventHandler().HandleWheelEvent(event);
  }

  if (event.phase == WebMouseWheelEvent::kPhaseMayBegin) {
    FadeInChainedScrollbarsAndDeferFadeOut();
    return WebInputEventResult::kNotHandled;
  }

  // Begin deferred fade-out when handling an event at any phase after
  // may-begin (e.g. began, changed, etc.).
  FadeOutScrollbarsIfNeeded();

  if (wheel_target_) {
    WheelEvent* dom_event =
        WheelEvent::Create(event, *wheel_target_->GetDocument().domWindow());

    // The event handler might remove |wheel_target_| from DOM so we should get
    // this value now (see https://crbug.com/857013).
    bool should_enforce_vertical_scroll =
        wheel_target_->GetDocument().IsVerticalScrollEnforced();
    DispatchEventResult dom_event_result =
        wheel_target_->DispatchEvent(*dom_event);
    if (dom_event_result != DispatchEventResult::kNotCanceled) {
      // Reset the target if the dom event is cancelled to make sure that new
      // targeting happens for the next wheel event.
      UpdateWheelTarget(nullptr);

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
  if (wheel_target_ == target) {
    UpdateWheelTarget(/*wheel_target=*/nullptr);
  }
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
  if (node && node->IsTextNode()) {
    // Find first ancestor that has layout object.
    for (node = FlatTreeTraversal::Parent(*node);
         node && !node->GetLayoutObject();
         node = FlatTreeTraversal::Parent(*node)) {
    }
  }

  // If we're over the frame scrollbar, scroll the document.
  if (!node && result.GetScrollbar())
    node = doc->documentElement();

  return node;
}

void MouseWheelEventManager::UpdateWheelTarget(Node* wheel_target) {
  if (wheel_target_ == wheel_target) {
    return;
  }

  if (LocalFrame* subframe =
          event_handling_util::SubframeForTargetNode(wheel_target_.Get())) {
    // Clear subframe wheel targets before updating wheel target.
    subframe->GetEventHandler().GetMouseWheelEventManager().UpdateWheelTarget(
        /*wheel_target=*/nullptr);
  } else {
    // To avoid missing deferred fade-out, begin deferred fade-out before
    // the leaf of wheel target chain is updated.
    FadeOutScrollbarsIfNeeded();
  }

  wheel_target_ = wheel_target;

  if (!wheel_target_) {
    if (auto* parent_frame = DynamicTo<LocalFrame>(frame_->Parent())) {
      parent_frame->GetEventHandler()
          .GetMouseWheelEventManager()
          .UpdateWheelTarget(nullptr);
    }
  }
}

void MouseWheelEventManager::FadeInChainedScrollbarsAndDeferFadeOut() {
  if (!base::FeatureList::IsEnabled(
          blink::features::kFadeInScrollbarWhenMouseWheelMayBegin)) {
    return;
  }
  constexpr int kLeft = 0x1 << 0;
  constexpr int kRight = 0x1 << 1;
  constexpr int kUp = 0x1 << 2;
  constexpr int kDown = 0x1 << 3;
  constexpr int kAllDirections = kLeft | kRight | kUp | kDown;

  int consumed = 0;

  for (ScrollableArea& scrollable : ScrollableAreaTraversal(wheel_target_)) {
    if (consumed == kAllDirections) {
      break;
    }

    ScrollOffset scroll_offset = scrollable.GetScrollOffset();
    ScrollOffset min_scroll_offset = scrollable.MinimumScrollOffset();
    ScrollOffset max_scroll_offset = scrollable.MaximumScrollOffset();

    bool horizontal = false;
    bool vertical = false;

    if (!(consumed & kLeft) && scroll_offset.x() > min_scroll_offset.x()) {
      horizontal = true;
      consumed |= kLeft;
    }

    if (!(consumed & kRight) && scroll_offset.x() < max_scroll_offset.x()) {
      horizontal = true;
      consumed |= kRight;
    }

    if (!(consumed & kUp) && scroll_offset.y() > min_scroll_offset.y()) {
      vertical = true;
      consumed |= kUp;
    }

    if (!(consumed & kDown) && scroll_offset.y() < max_scroll_offset.y()) {
      vertical = true;
      consumed |= kDown;
    }

    if (!horizontal && !vertical) {
      continue;
    }

    if (scrollable.FadeInScrollbarIfExists(horizontal, vertical)) {
      fade_out_deferred_scrollables_.insert(&scrollable);
    }
  }
}

inline void MouseWheelEventManager::FadeOutScrollbarsIfNeeded() {
  for (ScrollableArea* scrollable : fade_out_deferred_scrollables_) {
    if (scrollable) {
      scrollable->FadeOutScrollbarIfNeeded();
    }
  }
  fade_out_deferred_scrollables_.clear();
}

}  // namespace blink
