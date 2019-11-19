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

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_list_box.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

// Delay time in second for start autoscroll if pointer is in border edge of
// scrollable element.
constexpr base::TimeDelta kAutoscrollDelay = base::TimeDelta::FromSecondsD(0.2);

static const int kNoMiddleClickAutoscrollRadius = 15;

static const Cursor& MiddleClickAutoscrollCursor(const FloatSize& velocity,
                                                 bool scroll_vert,
                                                 bool scroll_horiz) {
  // At the original click location we draw a 4 arrowed icon. Over this icon
  // there won't be any scroll, So don't change the cursor over this area.
  bool east = velocity.Width() < 0;
  bool west = velocity.Width() > 0;
  bool north = velocity.Height() > 0;
  bool south = velocity.Height() < 0;

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

void AutoscrollController::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_);
}

bool AutoscrollController::SelectionAutoscrollInProgress() const {
  return autoscroll_type_ == kAutoscrollForSelection;
}

bool AutoscrollController::AutoscrollInProgress() const {
  return autoscroll_layout_object_;
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
  if (!scrollable)
    scrollable =
        layout_object->IsListBox() ? ToLayoutListBox(layout_object) : nullptr;
  if (!scrollable)
    return;

  pressed_layout_object_ = layout_object && layout_object->IsBox()
                               ? ToLayoutBox(layout_object)
                               : nullptr;
  autoscroll_type_ = kAutoscrollForSelection;
  autoscroll_layout_object_ = scrollable;
  ScheduleMainThreadAnimation();
}

void AutoscrollController::StopAutoscroll() {
  if (pressed_layout_object_) {
    pressed_layout_object_->StopAutoscroll();
    pressed_layout_object_ = nullptr;
  }
  autoscroll_layout_object_ = nullptr;
  autoscroll_type_ = kNoAutoscroll;
}

void AutoscrollController::StopAutoscrollIfNeeded(LayoutObject* layout_object) {
  if (pressed_layout_object_ == layout_object)
    pressed_layout_object_ = nullptr;

  if (autoscroll_layout_object_ != layout_object)
    return;
  autoscroll_layout_object_ = nullptr;
  autoscroll_type_ = kNoAutoscroll;
}

void AutoscrollController::UpdateAutoscrollLayoutObject() {
  if (!autoscroll_layout_object_)
    return;

  LayoutObject* layout_object = autoscroll_layout_object_;

  while (layout_object && !(layout_object->IsBox() &&
                            ToLayoutBox(layout_object)->CanAutoscroll()))
    layout_object = layout_object->Parent();

  autoscroll_layout_object_ = layout_object && layout_object->IsBox()
                                  ? ToLayoutBox(layout_object)
                                  : nullptr;

  if (!autoscroll_layout_object_)
    autoscroll_type_ = kNoAutoscroll;
}

void AutoscrollController::UpdateDragAndDrop(Node* drop_target_node,
                                             const FloatPoint& event_position,
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
      ->UpdateAllLifecyclePhasesExceptPaint();

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
      PhysicalOffset::FromFloatPointRound(event_position) + offset;

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

void AutoscrollController::HandleMouseMoveForMiddleClickAutoscroll(
    LocalFrame* frame,
    const FloatPoint& position_global,
    bool is_middle_button) {
  if (!MiddleClickAutoscrollInProgress())
    return;

  LocalFrameView* view = frame->View();
  if (!view)
    return;

  FloatSize distance =
      (position_global - middle_click_autoscroll_start_pos_global_)
          .ScaledBy(1 / frame->DevicePixelRatio());

  if (fabs(distance.Width()) <= kNoMiddleClickAutoscrollRadius)
    distance.SetWidth(0);
  if (fabs(distance.Height()) <= kNoMiddleClickAutoscrollRadius)
    distance.SetHeight(0);

  const float kExponent = 2.2f;
  const float kMultiplier = -0.000008f;
  const int x_signum = (distance.Width() < 0) ? -1 : (distance.Width() > 0);
  const int y_signum = (distance.Height() < 0) ? -1 : (distance.Height() > 0);
  FloatSize velocity(
      pow(fabs(distance.Width()), kExponent) * kMultiplier * x_signum,
      pow(fabs(distance.Height()), kExponent) * kMultiplier * y_signum);

  if (velocity != last_velocity_) {
    last_velocity_ = velocity;
    if (middle_click_mode_ == kMiddleClickInitial)
      middle_click_mode_ = kMiddleClickHolding;
    page_->GetChromeClient().SetCursorOverridden(false);
    view->SetCursor(MiddleClickAutoscrollCursor(
        velocity, can_scroll_vertically_, can_scroll_horizontally_));
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

  if (!frame->IsMainFrame())
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
  frame->LocalFrameRoot().GetEventHandler().ScheduleCursorUpdate();
}

bool AutoscrollController::MiddleClickAutoscrollInProgress() const {
  return autoscroll_type_ == kAutoscrollForMiddleClick;
}

void AutoscrollController::StartMiddleClickAutoscroll(
    LocalFrame* frame,
    const FloatPoint& position,
    const FloatPoint& position_global,
    bool scroll_vert,
    bool scroll_horiz) {
  DCHECK(RuntimeEnabledFeatures::MiddleClickAutoscrollEnabled());
  // We don't want to trigger the autoscroll or the middleClickAutoscroll if
  // it's already active.
  if (autoscroll_type_ != kNoAutoscroll)
    return;

  autoscroll_type_ = kAutoscrollForMiddleClick;
  middle_click_mode_ = kMiddleClickInitial;
  middle_click_autoscroll_start_pos_global_ = position_global;
  can_scroll_vertically_ = scroll_vert;
  can_scroll_horizontally_ = scroll_horiz;

  UseCounter::Count(frame->GetDocument(),
                    WebFeature::kMiddleClickAutoscrollStart);

  last_velocity_ = FloatSize();

  if (LocalFrameView* view = frame->View()) {
    view->SetCursor(MiddleClickAutoscrollCursor(
        last_velocity_, can_scroll_vertically_, can_scroll_horizontally_));
  }
  page_->GetChromeClient().SetCursorOverridden(true);
  page_->GetChromeClient().AutoscrollStart(
      position.ScaledBy(1 / frame->DevicePixelRatio()), frame);
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
      PhysicalOffset::FromFloatPointRound(
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

bool AutoscrollController::IsAutoscrolling() const {
  return (autoscroll_type_ != kNoAutoscroll);
}

}  // namespace blink
