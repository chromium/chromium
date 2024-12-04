// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scroll_marker_pseudo_element.h"

#include "cc/input/scroll_snap_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

void ScrollMarkerPseudoElement::DefaultEventHandler(Event& event) {
  bool is_click =
      event.IsMouseEvent() && event.type() == event_type_names::kClick;
  bool is_key_down =
      event.IsKeyboardEvent() && event.type() == event_type_names::kKeydown;
  bool is_enter_or_space =
      is_key_down && (To<KeyboardEvent>(event).keyCode() == VKEY_RETURN ||
                      To<KeyboardEvent>(event).keyCode() == VKEY_SPACE);
  bool is_left_or_up_arrow_key =
      is_key_down && (To<KeyboardEvent>(event).keyCode() == VKEY_LEFT ||
                      To<KeyboardEvent>(event).keyCode() == VKEY_UP);
  bool is_right_or_down_arrow_key =
      is_key_down && (To<KeyboardEvent>(event).keyCode() == VKEY_RIGHT ||
                      To<KeyboardEvent>(event).keyCode() == VKEY_DOWN);
  bool should_intercept =
      event.target() == this &&
      (is_click || is_enter_or_space || is_left_or_up_arrow_key ||
       is_right_or_down_arrow_key);
  if (should_intercept) {
    if (scroll_marker_group_) {
      if (is_right_or_down_arrow_key) {
        scroll_marker_group_->ActivateNextScrollMarker(/*focus=*/true);
      } else if (is_left_or_up_arrow_key) {
        scroll_marker_group_->ActivatePrevScrollMarker(/*focus=*/true);
      } else if (is_click || is_enter_or_space) {
        // parentElement is ::column for column scroll marker and
        // ultimate originating element for regular scroll marker.
        mojom::blink::ScrollIntoViewParamsPtr params =
            scroll_into_view_util::CreateScrollIntoViewParams(
                *parentElement()->GetComputedStyle());
        ScrollIntoViewNoVisualUpdate(std::move(params));
        scroll_marker_group_->SetSelected(*this);
      }
    }
    event.SetDefaultHandled();
  }
  PseudoElement::DefaultEventHandler(event);
}

void ScrollMarkerPseudoElement::SetScrollMarkerGroup(
    ScrollMarkerGroupPseudoElement* scroll_marker_group) {
  if (scroll_marker_group_ && scroll_marker_group_ != scroll_marker_group) {
    scroll_marker_group_->RemoveFromFocusGroup(*this);
  }
  scroll_marker_group_ = scroll_marker_group;
  if (scroll_marker_group) {
    scroll_marker_group->AddToFocusGroup(*this);
  }
}

void ScrollMarkerPseudoElement::SetSelected(bool value) {
  if (is_selected_ == value) {
    return;
  }
  is_selected_ = value;
  PseudoStateChanged(CSSSelector::kPseudoTargetCurrent);
}

void ScrollMarkerPseudoElement::AttachLayoutTree(AttachContext& context) {
  CHECK(context.parent);
  CHECK(context.parent->GetNode());

  if (auto* group = DynamicTo<ScrollMarkerGroupPseudoElement>(
          context.parent->GetNode())) {
    SetScrollMarkerGroup(group);
    PseudoElement::AttachLayoutTree(context);
    return;
  }

  // The layout box for these pseudo elements are attached to the
  // ::scroll-marker-group box during layout above. Make sure we walk any
  // ::scroll-marker child and clear dirty bits for the RebuildLayoutTree()
  // pass.
  ContainerNode::AttachLayoutTree(context);

  if (scroll_marker_group_) {
    if (LayoutObject* scroller_box = scroll_marker_group_->GetLayoutObject()
                                         ->ScrollerFromScrollMarkerGroup()) {
      // Mark the scroller for layout to make sure we repopulate the
      // ::scroll-marker-group box with ::scroll-marker boxes.
      scroller_box->SetNeedsLayoutAndFullPaintInvalidation(
          layout_invalidation_reason::kScrollMarkersChanged);
    }
  }
}

void ScrollMarkerPseudoElement::Dispose() {
  SetScrollMarkerGroup(nullptr);
  PseudoElement::Dispose();
}

void ScrollMarkerPseudoElement::Trace(Visitor* v) const {
  v->Trace(scroll_marker_group_);
  PseudoElement::Trace(v);
}

}  // namespace blink
