/*
 * Copyright (C) 2006, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/forms/spin_button_element.h"

#include "base/notreached.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace blink {

SpinButtonElement::SpinButtonElement(Document& document,
                                     SpinButtonOwner& spin_button_owner)
    : HTMLDivElement(document),
      spin_button_owner_(&spin_button_owner),
      capturing_(false),
      up_down_state_(kDown),
      press_starting_state_(kDown),
      should_recalc_up_down_state_(false),
      repeating_timer_(document.GetTaskRunner(TaskType::kInternalDefault),
                       this,
                       &SpinButtonElement::RepeatingTimerFired) {
  SetShadowPseudoId(AtomicString("-webkit-inner-spin-button"));
  setAttribute(html_names::kIdAttr, shadow_element_names::kIdSpinButton);
}

void SpinButtonElement::DetachLayoutTree(bool performing_reattach) {
  ReleaseCapture(kEventDispatchDisallowed);
  HTMLDivElement::DetachLayoutTree(performing_reattach);
}

void SpinButtonElement::DefaultEventHandler(Event& event) {
  auto* mouse_event = DynamicTo<MouseEvent>(event);
  if (!mouse_event) {
    if (!event.DefaultHandled())
      HTMLDivElement::DefaultEventHandler(event);
    return;
  }

  LayoutBox* box = GetLayoutBox();
  if (!box) {
    if (!event.DefaultHandled())
      HTMLDivElement::DefaultEventHandler(event);
    return;
  }

  if (!ShouldRespondToMouseEvents()) {
    if (!event.DefaultHandled())
      HTMLDivElement::DefaultEventHandler(event);
    return;
  }

  if (mouse_event->type() == event_type_names::kMousedown &&
      mouse_event->button() ==
          static_cast<int16_t>(WebPointerProperties::Button::kLeft)) {
      if (spin_button_owner_)
        spin_button_owner_->FocusAndSelectSpinButtonOwner();
      if (GetLayoutObject()) {
          // A JavaScript event handler called in doStepAction() below
          // might change the element state and we might need to
          // cancel the repeating timer by the state change. If we
          // started the timer after doStepAction(), we would have no
          // chance to cancel the timer.
          StartRepeatingTimer();
          if (should_recalc_up_down_state_) {
            should_recalc_up_down_state_ = false;
            CalculateUpDownStateByMouseLocation(event);
          }
          DoStepAction(up_down_state_ == kUp ? 1 : -1);
      }
      // Check |GetLayoutObject| again to make sure element is not removed by
      // |DoStepAction|
      if (GetLayoutObject() && !capturing_) {
        if (LocalFrame* frame = GetDocument().GetFrame()) {
          frame->GetEventHandler().SetPointerCapture(
              PointerEventFactory::kMouseId, this);
          capturing_ = true;
          if (Page* page = GetDocument().GetPage())
            page->GetChromeClient().RegisterPopupOpeningObserver(this);
        }
      }
      event.SetDefaultHandled();
  } else if (mouse_event->type() == event_type_names::kMouseup &&
             mouse_event->button() ==
                 static_cast<int16_t>(WebPointerProperties::Button::kLeft)) {
    ReleaseCapture();
  } else if (event.type() == event_type_names::kMousemove) {
    CalculateUpDownStateByMouseLocation(event);
  }

  if (!event.DefaultHandled())
    HTMLDivElement::DefaultEventHandler(event);
}

void SpinButtonElement::WillOpenPopup() {
  ReleaseCapture();
}

void SpinButtonElement::ForwardEvent(Event& event) {
  if (!GetLayoutBox())
    return;

  if (event.type() == event_type_names::kFocus)
    should_recalc_up_down_state_ = true;

  if (!event.HasInterface(event_interface_names::kWheelEvent))
    return;

  if (!spin_button_owner_)
    return;

  if (!spin_button_owner_->ShouldSpinButtonRespondToWheelEvents())
    return;

  DoStepAction(To<WheelEvent>(event).wheelDeltaY());
  event.SetDefaultHandled();
}

bool SpinButtonElement::WillRespondToMouseMoveEvents() const {
  if (GetLayoutBox() && ShouldRespondToMouseEvents())
    return true;

  return HTMLDivElement::WillRespondToMouseMoveEvents();
}

bool SpinButtonElement::WillRespondToMouseClickEvents() {
  if (GetLayoutBox() && ShouldRespondToMouseEvents())
    return true;

  return HTMLDivElement::WillRespondToMouseClickEvents();
}

void SpinButtonElement::DoStepAction(int amount) {
  if (!spin_button_owner_)
    return;

  if (amount > 0)
    spin_button_owner_->SpinButtonStepUp();
  else if (amount < 0)
    spin_button_owner_->SpinButtonStepDown();
}

void SpinButtonElement::ReleaseCapture(EventDispatch event_dispatch) {
  StopRepeatingTimer();
  if (!capturing_)
    return;
  if (LocalFrame* frame = GetDocument().GetFrame()) {
    frame->GetEventHandler().ReleasePointerCapture(
        PointerEventFactory::kMouseId, this);
    capturing_ = false;
    if (Page* page = GetDocument().GetPage())
      page->GetChromeClient().UnregisterPopupOpeningObserver(this);
  }
  if (spin_button_owner_)
    spin_button_owner_->SpinButtonDidReleaseMouseCapture(event_dispatch);
}

bool SpinButtonElement::MatchesReadOnlyPseudoClass() const {
  return OwnerShadowHost()->MatchesReadOnlyPseudoClass();
}

bool SpinButtonElement::MatchesReadWritePseudoClass() const {
  return OwnerShadowHost()->MatchesReadWritePseudoClass();
}

void SpinButtonElement::StartRepeatingTimer() {
  press_starting_state_ = up_down_state_;
  Page* page = GetDocument().GetPage();
  DCHECK(page);
  ScrollbarTheme& theme = page->GetScrollbarTheme();
  repeating_timer_.Start(theme.InitialAutoscrollTimerDelay(),
                         theme.AutoscrollTimerDelay(), FROM_HERE);
}

void SpinButtonElement::StopRepeatingTimer() {
  repeating_timer_.Stop();
}

void SpinButtonElement::Step(int amount) {
  if (!ShouldRespondToMouseEvents())
    return;
  DoStepAction(amount);
}

void SpinButtonElement::RepeatingTimerFired(TimerBase*) {
    Step(up_down_state_ == kUp ? 1 : -1);
}

bool SpinButtonElement::ShouldRespondToMouseEvents() const {
  return !spin_button_owner_ ||
         spin_button_owner_->ShouldSpinButtonRespondToMouseEvents();
}

void SpinButtonElement::CalculateUpDownStateByMouseLocation(Event& event) {
  auto* mouse_event = DynamicTo<MouseEvent>(event);
  LayoutBox* box = GetLayoutBox();
  if (!mouse_event || !box)
    return;

  gfx::Point local = gfx::ToRoundedPoint(
      box->AbsoluteToLocalPoint(mouse_event->AbsoluteLocation()));
  UpDownState old_up_down_state = up_down_state_;
  WritingDirectionMode writing_direction =
      GetComputedStyle() ? GetComputedStyle()->GetWritingDirection()
                         : WritingDirectionMode(WritingMode::kHorizontalTb,
                                                TextDirection::kLtr);
  switch (writing_direction.LineOver()) {
    case PhysicalDirection::kUp:
      up_down_state_ = (local.y() < box->Size().height / 2) ? kUp : kDown;
      break;
    case PhysicalDirection::kDown:
      NOTREACHED();
    case PhysicalDirection::kLeft:
      up_down_state_ = (local.x() < box->Size().width / 2) ? kUp : kDown;
      break;
    case PhysicalDirection::kRight:
      up_down_state_ = (local.x() < box->Size().width / 2) ? kDown : kUp;
      break;
  }
  if (up_down_state_ != old_up_down_state)
    GetLayoutObject()->SetShouldDoFullPaintInvalidation();
}

void SpinButtonElement::Trace(Visitor* visitor) const {
  visitor->Trace(spin_button_owner_);
  visitor->Trace(repeating_timer_);
  HTMLDivElement::Trace(visitor);
}

}  // namespace blink
