/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2012 Digia Plc. and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/page/autoscroll_controller.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/scroll_manager.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/cursors.h"
#include "ui/base/cursor/cursor.h"

namespace blink {

// Delay time in second for start autoscroll if pointer is in border edge of
// scrollable element.
constexpr base::TimeDelta kAutoscrollDelay = base::Seconds(0.2);

static const int kNoMiddleClickAutoscrollRadius = 15;

static const ui::Cursor& MiddleClickAutoscrollCursor(
    const gfx::Vector2dF& velocity,
    bool scroll_vert,
    bool scroll_horiz) {
  // At the original click location we draw a 4 arrowed icon. Over this icon
  // there won't be any scroll, So don't change the cursor over this area.
  bool east = velocity.x() < 0;
  bool west = velocity.x() > 0;
  bool north = velocity.y() > 0;
  bool south = velocity.y() < 0;

  if (north && scroll_vert) {
    if (scroll_horiz) {
      if (east)
        return NorthEastPanningCursor();
      if (west)
        return NorthWestPanningCursor();
    }
    return NorthPanningCursor();
  }
  if (south && scroll_vert) {
    if (scroll_horiz) {
      if (east)
        return SouthEastPanningCursor();
      if (west)
        return SouthWestPanningCursor();
    }
    return SouthPanningCursor();
  }
  if (east && scroll_horiz)
    return EastPanningCursor();
  if (west && scroll_horiz)
    return WestPanningCursor();
  if (scroll_vert && !scroll_horiz)
    return MiddlePanningVerticalCursor();
  if (scroll_horiz && !scroll_vert)
    return MiddlePanningHorizontalCursor();
  return MiddlePanningCursor();
}

AutoscrollController::AutoscrollController(Page& page) : page_(&page) {}

void AutoscrollController::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
  visitor->Trace(autoscroll_layout_object_);
  visitor->Trace(pressed_layout_object_);
  visitor->Trace(horizontal_autoscroll_layout_box_);
  visitor->Trace(vertical_autoscroll_layout_box_);
}

bool AutoscrollController::SelectionAutoscrollInProgress() const {
  return autoscroll_type_ == kAutoscrollForSelection;
}

bool AutoscrollController::AutoscrollInProgress() const {
  return autoscroll_layout_object_ != nullptr;
}

bool AutoscrollController::AutoscrollInProgressFor(
    const LayoutBox* layout_object) const {
  return autoscroll_layout_object_ == layout_object;
}

void AutoscrollController::StartAutoscrollForSelection(
    LayoutObject* layout_object) {
  // We don't want to trigger the autoscroll or the middleClickAutoscroll if
  // it's already active.
  if (autoscroll_type_ != kNoAutoscroll)
    return;
  LayoutBox* scrollable = LayoutBox::FindAutoscrollable(
      layout_object, /*is_middle_click_autoscroll*/ false);
  if (!scrollable && layout_object->GetNode()) {
    scrollable = layout_object->GetNode()->AutoscrollBox();
  }
  if (!scrollable)
    return;

  pressed_layout_object_ = DynamicTo<LayoutBox>(layout_object);
  autoscroll_type_ = kAutoscrollForSelection;
  autoscroll_layout_object_ = scrollable;
  UpdateCachedAutoscrollForSelectionState(true);
  ScheduleMainThreadAnimation();
}

void AutoscrollController::StopAutoscroll() {
  if (pressed_layout_object_) {
    if (pressed_layout_object_->GetNode())
      pressed_layout_object_->GetNode()->StopAutoscroll();
    pressed_layout_object_ = nullptr;
  }
  UpdateCachedAutoscrollForSelectionState(false);
  autoscroll_layout_object_ = nullptr;
  autoscroll_type_ = kNoAutoscroll;
}

void AutoscrollController::StopAutoscrollIfNeeded(LayoutObject* layout_object) {
  if (pressed_layout_object_ == layout_object)
    pressed_layout_object_ = nullptr;

  if (horizontal_autoscroll_layout_box_ == layout_object)
    horizontal_autoscroll_layout_box_ = nullptr;

  if (vertical_autoscroll_layout_box_ == layout_object)
    vertical_autoscroll_layout_box_ = nullptr;

  if (MiddleClickAutoscrollInProgress() && !horizontal_autoscroll_layout_box_ &&
      !vertical_autoscroll_layout_box_) {
    page_->GetChromeClient().AutoscrollEnd(layout_object->GetFrame());
    autoscroll_type_ = kNoAutoscroll;
  }

  if (autoscroll_layout_object_ != layout_object)
    return;
  UpdateCachedAutoscrollForSelectionState(false);
  autoscroll_layout_object_ = nullptr;
  autoscroll_type_ = kNoAutoscroll;
}

void AutoscrollController::UpdateDragAndDrop(Node* drop_target_node,
                                             const gfx::PointF& event_position,
                                             base::TimeTicks event_time) {
  if (!drop_target_node || !drop_target_node->GetLayoutObject()) {
    StopAutoscroll();
    return;
  }

  if (autoscroll_layout_object_ &&
      autoscroll_layout_object_->GetFrame() !=
          drop_target_node->GetLayoutObject()->GetFrame())
    return;

  drop_target_node->GetLayoutObject()
      ->GetFrameView()
      ->UpdateAllLifecyclePhasesExceptPaint(DocumentUpdateReason::kScroll);

  LayoutBox* scrollable =
      LayoutBox::FindAutoscrollable(drop_target_node->GetLayoutObject(),
                                    /*is_middle_click_autoscroll*/ false);
  if (!scrollable) {
    StopAutoscroll();
    return;
  }

  Page* page =
      scrollable->GetFrame() ? scrollable->GetFrame()->GetPage() : nullptr;
  if (!page) {
    StopAutoscroll();
    return;
  }

  PhysicalOffset offset =
      scrollable->CalculateAutoscrollDirection(event_position);
  if (offset.IsZero()) {
    StopAutoscroll();
    return;
  }

  drag_and_drop_autoscroll_reference_position_ =
      PhysicalOffset::FromPointFRound(event_position) + offset;

  if (autoscroll_type_ == kNoAutoscroll) {
    autoscroll_type_ = kAutoscrollForDragAndDrop;
    autoscroll_layout_object_ = scrollable;
    drag_and_drop_autoscroll_start_time_ = event_time;
    UseCounter::Count(drop_target_node->GetDocument(),
                      WebFeature::kDragAndDropScrollStart);
    ScheduleMainThreadAnimation();
  } else if (autoscroll_layout_object_ != scrollable) {
    drag_and_drop_autoscroll_start_time_ = event_time;
    autoscroll_layout_object_ = scrollable;
  }
}

bool CanScrollDirection(LayoutBox* layout_box,
                        Page* page,
                        ScrollOrientation orientation) {
  DCHECK(layout_box);

  bool can_scroll = orientation == ScrollOrientation::kHorizontalScroll
                        ? layout_box->HasScrollableOverflowX()
                        : layout_box->HasScrollableOverflowY();

  if (page) {
    // TODO: Consider only doing this when the layout_box is the document to
    // correctly handle autoscrolling a DIV when pinch-zoomed.
    // See comments on crrev.com/c/2109286
    ScrollOffset maximum_scroll_offset =
        page->GetVisualViewport().MaximumScrollOffset();
    can_scroll =
        can_scroll || (orientation == ScrollOrientation::kHorizontalScroll
                           ? maximum_scroll_offset.x() > 0
                           : maximum_scroll_offset.y() > 0);
  }

  return can_scroll;
}

void AutoscrollController::HandleMouseMoveForMiddleClickAutoscroll(
    LocalFrame* frame,
    const gfx::PointF& position_global,
    bool is_middle_button) {
  if (!MiddleClickAutoscrollInProgress())
    return;

  bool horizontal_autoscroll_possible =
      horizontal_autoscroll_layout_box_ &&
      horizontal_autoscroll_layout_box_->GetNode();
  bool vertical_autoscroll_possible =
      vertical_autoscroll_layout_box_ &&
      vertical_autoscroll_layout_box_->GetNode();
  if (horizontal_autoscroll_possible &&
      !horizontal_autoscroll_layout_box_->IsUserScrollable() &&
      vertical_autoscroll_possible &&
      !vertical_autoscroll_layout_box_->IsUserScrollable()) {
    StopMiddleClickAutoscroll(frame);
    return;
  }

  LocalFrameView* view = frame->View();
  if (!view)
    return;

  gfx::Vector2dF distance = gfx::ScaleVector2d(
      position_global - middle_click_autoscroll_start_pos_global_,
      1 / frame->DevicePixelRatio());

  if (fabs(distance.x()) <= kNoMiddleClickAutoscrollRadius)
    distance.set_x(0);
  if (fabs(distance.y()) <= kNoMiddleClickAutoscrollRadius)
    distance.set_y(0);

  const float kExponent = 2.2f;
  const float kMultiplier = -0.000008f;
  const int x_signum = (distance.x() < 0) ? -1 : (distance.x() > 0);
  const int y_signum = (distance.y() < 0) ? -1 : (distance.y() > 0);
  gfx::Vector2dF velocity(
      pow(fabs(distance.x()), kExponent) * kMultiplier * x_signum,
      pow(fabs(distance.y()), kExponent) * kMultiplier * y_signum);

  bool can_scroll_vertically =
      vertical_autoscroll_possible
          ? CanScrollDirection(vertical_autoscroll_layout_box_,
                               frame->GetPage(),
                               ScrollOrientation::kVerticalScroll)
          : false;
  bool can_scroll_horizontally =
      horizontal_autoscroll_possible
          ? CanScrollDirection(horizontal_autoscroll_layout_box_,
                               frame->GetPage(),
                               ScrollOrientation::kHorizontalScroll)
          : false;

  if (velocity != last_velocity_) {
    last_velocity_ = velocity;
    if (middle_click_mode_ == kMiddleClickInitial)
      middle_click_mode_ = kMiddleClickHolding;
    page_->GetChromeClient().SetCursorOverridden(false);
    view->SetCursor(MiddleClickAutoscrollCursor(velocity, can_scroll_vertically,
                                                can_scroll_horizontally));
    page_->GetChromeClient().SetCursorOverridden(true);
    page_->GetChromeClient().AutoscrollFling(velocity, frame);
  }
}

void AutoscrollController::HandleMouseReleaseForMiddleClickAutoscroll(
    LocalFrame* frame,
    bool is_middle_button) {
  DCHECK(RuntimeEnabledFeatures::MiddleClickAutoscrollEnabled());
  if (!MiddleClickAutoscrollInProgress())
    return;

  // We only want to execute this event once per event dispatch loop so
  // we restrict to processing it only on the local root.
  if (!frame->IsLocalRoot())
    return;

  if (middle_click_mode_ == kMiddleClickInitial && is_middle_button)
    middle_click_mode_ = kMiddleClickToggled;
  else if (middle_click_mode_ == kMiddleClickHolding)
    StopMiddleClickAutoscroll(frame);
}

void AutoscrollController::StopMiddleClickAutoscroll(LocalFrame* frame) {
  if (!MiddleClickAutoscrollInProgress())
    return;

  page_->GetChromeClient().AutoscrollEnd(frame);
  autoscroll_type_ = kNoAutoscroll;
  page_->GetChromeClient().SetCursorOverridden(false);
  frame->LocalFrameRoot().GetEventHandler().UpdateCursor();
  horizontal_autoscroll_layout_box_ = nullptr;
  vertical_autoscroll_layout_box_ = nullptr;
}

bool AutoscrollController::MiddleClickAutoscrollInProgress() const {
  return autoscroll_type_ == kAutoscrollForMiddleClick;
}

void AutoscrollController::StartMiddleClickAutoscroll(
    LocalFrame* frame,
    LayoutBox* scrollable,
    const gfx::PointF& position,
    const gfx::PointF& position_global) {
  DCHECK(RuntimeEnabledFeatures::MiddleClickAutoscrollEnabled());
  DCHECK(scrollable);
  // We don't want to trigger the autoscroll or the middleClickAutoscroll if
  // it's already active.
  if (autoscroll_type_ != kNoAutoscroll)
    return;

  autoscroll_type_ = kAutoscrollForMiddleClick;
  middle_click_mode_ = kMiddleClickInitial;
  middle_click_autoscroll_start_pos_global_ = position_global;

  bool can_scroll_vertically = false;
  bool can_scroll_horizontally = false;

  // Scroll propagation can be prevented in either direction independently.
  // We check whether autoscroll can be prevented in either direction after
  // checking whether the layout box can be scrolled. If propagation is not
  // allowed, we do not perform further checks for whether parents can be
  // scrolled in that direction.
  bool can_propagate_vertically = true;
  bool can_propagate_horizontally = true;

  LayoutObject* layout_object = scrollable;

  while (layout_object && !(can_scroll_horizontally && can_scroll_vertically)) {
    if (LayoutBox* layout_box = DynamicTo<LayoutBox>(layout_object)) {
      // Check whether the layout box can be scrolled and has horizontal
      // scrollable area.
      if (can_propagate_vertically &&
          CanScrollDirection(layout_box, frame->GetPage(),
                             ScrollOrientation::kVerticalScroll) &&
          !vertical_autoscroll_layout_box_) {
        vertical_autoscroll_layout_box_ = layout_box;
        can_scroll_vertically = true;
      }
      // Check whether the layout box can be scrolled and has vertical
      // scrollable area.
      if (can_propagate_horizontally &&
          CanScrollDirection(layout_box, frame->GetPage(),
                             ScrollOrientation::kHorizontalScroll) &&
          !horizontal_autoscroll_layout_box_) {
        horizontal_autoscroll_layout_box_ = layout_box;
        can_scroll_horizontally = true;
      }

      can_propagate_vertically = ScrollManager::CanPropagate(
          layout_box, ScrollPropagationDirection::kVertical);
      can_propagate_horizontally = ScrollManager::CanPropagate(
          layout_box, ScrollPropagationDirection::kHorizontal);
    }

    // Exit loop if we can't propagate to the parent in any direction or if
    // layout boxes have been found for both directions.
    if ((!can_propagate_vertically && !can_propagate_horizontally) ||
        (can_scroll_horizontally && can_scroll_vertically))
      break;

    if (!layout_object->Parent() &&
        layout_object->GetNode() == layout_object->GetDocument() &&
        layout_object->GetDocument().LocalOwner()) {
      layout_object =
          layout_object->GetDocument().LocalOwner()->GetLayoutObject();
    } else {
      layout_object = layout_object->Parent();
    }
  }

  UseCounter::Count(frame->GetDocument(),
                    WebFeature::kMiddleClickAutoscrollStart);

  last_velocity_ = gfx::Vector2dF();

  if (LocalFrameView* view = frame->View()) {
    view->SetCursor(MiddleClickAutoscrollCursor(
        last_velocity_, can_scroll_vertically, can_scroll_horizontally));
  }
  page_->GetChromeClient().SetCursorOverridden(true);
  page_->GetChromeClient().AutoscrollStart(
      gfx::ScalePoint(position, 1 / frame->DevicePixelRatio()), frame);
}

void AutoscrollController::Animate() {
  // Middle-click autoscroll isn't handled on the main thread.
  if (MiddleClickAutoscrollInProgress())
    return;

  if (!autoscroll_layout_object_ || !autoscroll_layout_object_->GetFrame()) {
    StopAutoscroll();
    return;
  }

  EventHandler& event_handler =
      autoscroll_layout_object_->GetFrame()->GetEventHandler();
  PhysicalOffset offset =
      autoscroll_layout_object_->CalculateAutoscrollDirection(
          event_handler.LastKnownMousePositionInRootFrame());
  PhysicalOffset selection_point =
      PhysicalOffset::FromPointFRound(
          event_handler.LastKnownMousePositionInRootFrame()) +
      offset;
  switch (autoscroll_type_) {
    case kAutoscrollForDragAndDrop:
      ScheduleMainThreadAnimation();
      if ((base::TimeTicks::Now() - drag_and_drop_autoscroll_start_time_) >
          kAutoscrollDelay)
        autoscroll_layout_object_->Autoscroll(
            drag_and_drop_autoscroll_reference_position_);
      break;
    case kAutoscrollForSelection:
      if (!event_handler.MousePressed()) {
        StopAutoscroll();
        return;
      }
      event_handler.UpdateSelectionForMouseDrag();

      // UpdateSelectionForMouseDrag may call layout to cancel auto scroll
      // animation.
      if (autoscroll_type_ != kNoAutoscroll) {
        DCHECK(autoscroll_layout_object_);
        ScheduleMainThreadAnimation();
        autoscroll_layout_object_->Autoscroll(selection_point);
      }
      break;
    case kNoAutoscroll:
    case kAutoscrollForMiddleClick:
      break;
  }
}

void AutoscrollController::ScheduleMainThreadAnimation() {
  page_->GetChromeClient().ScheduleAnimation(
      autoscroll_layout_object_->GetFrame()->View());
}

void AutoscrollController::UpdateCachedAutoscrollForSelectionState(
    bool autoscroll_selection) {
  if (!autoscroll_layout_object_ || !autoscroll_layout_object_->GetFrame() ||
      !autoscroll_layout_object_->GetFrame()->IsAttached() ||
      !autoscroll_layout_object_->GetFrame()->IsOutermostMainFrame()) {
    return;
  }
  autoscroll_layout_object_->GetFrame()
      ->LocalFrameRoot()
      .Client()
      ->NotifyAutoscrollForSelectionInMainFrame(autoscroll_selection);
}

bool AutoscrollController::IsAutoscrolling() const {
  return (autoscroll_type_ != kNoAutoscroll);
}

}  // namespace blink
