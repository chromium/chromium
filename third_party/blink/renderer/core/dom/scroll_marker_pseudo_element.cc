// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scroll_marker_pseudo_element.h"

#include "cc/input/scroll_snap_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

ScrollMarkerPseudoElement::ScrollMarkerPseudoElement(
    Element* originating_element)
    : PseudoElement(originating_element, kPseudoIdScrollMarker) {
  SetTabIndexExplicitly();
  UseCounter::Count(GetDocument(), WebFeature::kScrollMarkerPseudoElement);
}

bool ShouldSnapToAreaHorizontally(LayoutBox* container,
                                  cc::ScrollSnapType container_snap_type,
                                  cc::ScrollSnapAlign area_snap_align) {
  return area_snap_align.alignment_inline != cc::SnapAlignment::kNone &&
         (container_snap_type.axis == cc::SnapAxis::kBoth ||
          container_snap_type.axis == cc::SnapAxis::kX ||
          ((container_snap_type.axis == cc::SnapAxis::kInline &&
            container->IsHorizontalWritingMode()) ||
           (container_snap_type.axis == cc::SnapAxis::kBlock &&
            !container->IsHorizontalWritingMode())));
}

bool ShouldSnapToAreaVertically(LayoutBox* container,
                                cc::ScrollSnapType container_snap_type,
                                cc::ScrollSnapAlign area_snap_align) {
  return area_snap_align.alignment_inline != cc::SnapAlignment::kNone &&
         (container_snap_type.axis == cc::SnapAxis::kBoth ||
          container_snap_type.axis == cc::SnapAxis::kY ||
          ((container_snap_type.axis == cc::SnapAxis::kBlock &&
            container->IsHorizontalWritingMode()) ||
           (container_snap_type.axis == cc::SnapAxis::kInline &&
            !container->IsHorizontalWritingMode())));
}

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
      event.RawTarget() == this &&
      (is_click || is_enter_or_space || is_left_or_up_arrow_key ||
       is_right_or_down_arrow_key);
  if (should_intercept) {
    if (scroll_marker_group_) {
      if (is_right_or_down_arrow_key) {
        scroll_marker_group_->ActivateNextScrollMarker();
      } else if (is_left_or_up_arrow_key) {
        scroll_marker_group_->ActivatePrevScrollMarker();
      } else if (is_click || is_enter_or_space) {
        // parentElement is ::column for column scroll marker and
        // ultimate originating element for regular scroll marker.
        //
        // For a click, we want to minimize the active marker's movement away
        // from the user's mouse. So, let the snap code pick the closest snap
        // target that lets the active marker stay in view.
        scroll_marker_group_->ActivateScrollMarker(this, !is_click);
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

void ScrollMarkerPseudoElement::SetSelected(bool value,
                                            bool apply_snap_alignment) {
  if (is_selected_ == value) {
    return;
  }
  is_selected_ = value;
  PseudoStateChanged(CSSSelector::kPseudoTargetCurrent);
  if (ScrollMarkerGroup()) {
    const bool tabs_mode = ScrollMarkerGroup()->ScrollMarkerGroupMode() ==
                           ScrollMarkerGroup::ScrollMarkerMode::kTabs;
    if (RuntimeEnabledFeatures::CSSScrollMarkerGroupModesEnabled() &&
        tabs_mode) {
      // Update accessibility tree. Only active ::scroll-marker's ultimate
      // originating element and its content are in the tree, when in tabs mode.
      if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
        Element* scroller = ScrollMarkerGroup()->parentElement();
        cache->HandleScrollMarkerTabSelectionChanged(scroller);
      }
    }
  }
  if (is_selected_ && scroll_marker_group_) {
    if (LayoutBox* group_box = scroll_marker_group_->GetLayoutBox()) {
      // We defer executing the scroll here in case we are in a lifecycle phase
      // in which we shouldn't access
      // AbsoluteBoundingBoxRectHandlingEmptyInline (See ScrollIntoView below).
      // TODO(crbug.com/402772751): Should we be able to just update style and
      // layout and run the scroll here.
      group_box->GetFrameView()->AddPendingScrollMarkerSelectionUpdate(
          scroll_marker_group_, apply_snap_alignment);
      // Ensure that the scroll we've just queued is eventually executed by a
      // future lifecycle update.
      if (!group_box->GetFrameView()->IsUpdatingLifecycle()) {
        group_box->GetFrameView()->ScheduleAnimation();
      }
    }
  }
}

void ScrollMarkerPseudoElement::ScrollIntoView(bool apply_snap_alignment) {
  if (is_selected_ && scroll_marker_group_) {
    LayoutBox* group_box = scroll_marker_group_->GetLayoutBox();
    LayoutObject* marker_object = GetLayoutObject();
    if (!group_box || !marker_object) {
      return;
    }
    ScrollableArea* group_scroller = group_box->GetScrollableArea();
    if (group_scroller) {
      // AbsoluteBoundingBoxRectForScrollIntoView detects that this is a
      // scroll-marker pseudo and returns the rect of the originating element.
      // Since what we want is the rect of the scroll-marker itself, we use
      // AbsoluteBoundingBoxRectHandlingEmptyInline directly.
      PhysicalRect rect =
          marker_object->AbsoluteBoundingBoxRectHandlingEmptyInline();
      PhysicalBoxStrut scroll_margin =
          marker_object->Style()->ScrollMarginStrut();
      // Default to bringing the scroll-marker just into view at the nearest
      // edge.
      auto align_x = ScrollAlignment::ToEdgeIfNeeded();
      auto align_y = ScrollAlignment::ToEdgeIfNeeded();
      const auto group_snap_type = group_box->Style()->GetScrollSnapType();
      // Update the alignment if the group is a snap container and the marker is
      // a snap area.
      if (apply_snap_alignment && !group_snap_type.is_none) {
        const auto marker_snap_align =
            marker_object->Style()->GetScrollSnapAlign();

        if (ShouldSnapToAreaHorizontally(group_box, group_snap_type,
                                         marker_snap_align)) {
          align_x = scroll_into_view_util::PhysicalAlignmentFromSnapAlignStyle(
              *marker_object, kHorizontalScroll);
        }
        if (ShouldSnapToAreaVertically(group_box, group_snap_type,
                                       marker_snap_align)) {
          align_y = scroll_into_view_util::PhysicalAlignmentFromSnapAlignStyle(
              *marker_object, kVerticalScroll);
        }
      }
      mojom::blink::ScrollIntoViewParamsPtr params =
          scroll_into_view_util::CreateScrollIntoViewParams(align_x, align_y);
      params->behavior = group_box->Style()->GetScrollBehavior();
      // Indicate that this is for a scroll sequence so the ScrollIntoView uses
      // the requested behavior.
      // TODO(397989214): is_for_scroll_sequence might be obsolete as we no
      // longer perform ScrollIntoView in sequence. We should delete
      // or rename it.
      params->is_for_scroll_sequence = true;
      group_scroller->ScrollIntoView(rect, scroll_margin, params);
    }
  }
}

void ScrollMarkerPseudoElement::AttachLayoutTree(AttachContext& context) {
  if (context.parent) {
    if (auto* group = DynamicTo<ScrollMarkerGroupPseudoElement>(
            context.parent->GetNode())) {
      SetScrollMarkerGroup(group);
      PseudoElement::AttachLayoutTree(context);
      return;
    }
  }

  // The layout box for these pseudo-elements are attached to the
  // ::scroll-marker-group box during layout above. Make sure we walk any
  // ::scroll-marker child and clear dirty bits for the RebuildLayoutTree()
  // pass.
  ContainerNode::AttachLayoutTree(context);

  if (scroll_marker_group_) {
    if (LayoutObject* scroller_box = scroll_marker_group_->GetLayoutBox()
                                         ->ScrollerFromScrollMarkerGroup()) {
      // Mark the scroller for layout to make sure we repopulate the
      // ::scroll-marker-group box with ::scroll-marker boxes.
      scroller_box->SetNeedsLayoutAndFullPaintInvalidation(
          layout_invalidation_reason::kScrollMarkersChanged);
    }
  }
}

void ScrollMarkerPseudoElement::SetHasFocusWithinUpToAncestor(
    bool has_focus_within,
    Element* ancestor,
    bool need_snap_container_search) {
  DCHECK(scroll_marker_group_);
  if (this != ancestor) {
    SetHasFocusWithin(has_focus_within);
    FocusWithinStateChanged();
  }
  scroll_marker_group_->SetHasFocusWithinUpToAncestor(
      has_focus_within, ancestor, need_snap_container_search);
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
