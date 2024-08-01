/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/forms/slider_thumb_element.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/step_range.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/flex/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "ui/base/ui_base_features.h"

namespace blink {

SliderThumbElement::SliderThumbElement(Document& document)
    : HTMLDivElement(document), in_drag_mode_(false) {
  SetHasCustomStyleCallbacks();
  setAttribute(html_names::kIdAttr, shadow_element_names::kIdSliderThumb);
}

void SliderThumbElement::SetPositionFromValue() {
  // Since the code to calculate position is in the LayoutSliderThumb layout
  // path, we don't actually update the value here. Instead, we poke at the
  // layoutObject directly to trigger layout.
  if (GetLayoutObject()) {
    GetLayoutObject()->SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kSliderValueChanged);
    HTMLInputElement* input(HostInput());
    if (input && input->GetLayoutObject()) {
      // the slider track selected value needs to be updated.
      input->GetLayoutObject()->SetShouldDoFullPaintInvalidation();
    }
  }
}

LayoutObject* SliderThumbElement::CreateLayoutObject(
    const ComputedStyle& style) {
  return MakeGarbageCollected<LayoutBlockFlow>(this);
}

bool SliderThumbElement::IsDisabledFormControl() const {
  return HostInput() && HostInput()->IsDisabledFormControl();
}

bool SliderThumbElement::MatchesReadOnlyPseudoClass() const {
  return HostInput() && HostInput()->MatchesReadOnlyPseudoClass();
}

bool SliderThumbElement::MatchesReadWritePseudoClass() const {
  return HostInput() && HostInput()->MatchesReadWritePseudoClass();
}

void SliderThumbElement::DragFrom(const PhysicalOffset& point) {
  StartDragging();
  SetPositionFromPoint(point);
}

void SliderThumbElement::SetPositionFromPoint(const PhysicalOffset& point) {
  HTMLInputElement* input(HostInput());
  Element* track_element = input->EnsureShadowSubtree()->getElementById(
      shadow_element_names::kIdSliderTrack);

  const LayoutObject* input_object = input->GetLayoutObject();
  const LayoutBox* thumb_box = GetLayoutBox();
  const LayoutBox* track_box = track_element->GetLayoutBox();
  if (!input_object || !thumb_box || !track_box)
    return;

  PhysicalOffset point_in_track = track_box->AbsoluteToLocalPoint(point);
  auto writing_direction = thumb_box->StyleRef().GetWritingDirection();
  bool is_flipped = writing_direction.IsFlippedInlines();
  LayoutUnit track_size;
  LayoutUnit position;
  LayoutUnit current_position;
  const auto* input_box = To<LayoutBox>(input_object);
  PhysicalOffset thumb_offset =
      thumb_box->LocalToAncestorPoint(PhysicalOffset(), input_box) -
      track_box->LocalToAncestorPoint(PhysicalOffset(), input_box);
  if (!writing_direction.IsHorizontal()) {
    track_size = track_box->ContentHeight() - thumb_box->Size().height;
    position = point_in_track.top - thumb_box->Size().height / 2;
    position -= is_flipped ? thumb_box->MarginBottom() : thumb_box->MarginTop();
    current_position = thumb_offset.top;
  } else {
    track_size = track_box->ContentWidth() - thumb_box->Size().width;
    position = point_in_track.left - thumb_box->Size().width / 2;
    position -= is_flipped ? thumb_box->MarginRight() : thumb_box->MarginLeft();
    current_position = thumb_offset.left;
  }
  position = std::min(position, track_size).ClampNegativeToZero();
  const Decimal ratio =
      Decimal::FromDouble(static_cast<double>(position) / track_size);
  const Decimal fraction = is_flipped ? Decimal(1) - ratio : ratio;
  StepRange step_range(input->CreateStepRange(kRejectAny));
  Decimal value =
      step_range.ClampValue(step_range.ValueFromProportion(fraction));

  Decimal closest = input->FindClosestTickMarkValue(value);
  if (closest.IsFinite()) {
    double closest_fraction =
        step_range.ProportionFromValue(closest).ToDouble();
    double closest_ratio =
        is_flipped ? 1.0 - closest_fraction : closest_fraction;
    LayoutUnit closest_position(track_size * closest_ratio);
    const LayoutUnit snapping_threshold(5);
    if ((closest_position - position).Abs() <= snapping_threshold)
      value = closest;
  }

  String value_string = SerializeForNumberType(value);
  if (value_string == input->Value())
    return;

  // FIXME: This is no longer being set from renderer. Consider updating the
  // method name.
  input->SetValueFromRenderer(value_string);
  SetPositionFromValue();
}

void SliderThumbElement::StartDragging() {
  if (LocalFrame* frame = GetDocument().GetFrame()) {
    // Note that we get to here only we through mouse event path. The touch
    // events are implicitly captured to the starting element and will be
    // handled in handleTouchEvent function.
    frame->GetEventHandler().SetPointerCapture(PointerEventFactory::kMouseId,
                                               this);
    in_drag_mode_ = true;
  }
}

void SliderThumbElement::StopDragging() {
  if (!in_drag_mode_)
    return;

  if (LocalFrame* frame = GetDocument().GetFrame()) {
    frame->GetEventHandler().ReleasePointerCapture(
        PointerEventFactory::kMouseId, this);
  }
  in_drag_mode_ = false;
  if (GetLayoutObject()) {
    GetLayoutObject()->SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kSliderValueChanged);
  }
  if (HostInput())
    HostInput()->DispatchFormControlChangeEvent();
}

void SliderThumbElement::DefaultEventHandler(Event& event) {
  if (IsA<PointerEvent>(event) &&
      event.type() == event_type_names::kLostpointercapture) {
    StopDragging();
    return;
  }

  if (!IsA<MouseEvent>(event)) {
    HTMLDivElement::DefaultEventHandler(event);
    return;
  }

  // FIXME: Should handle this readonly/disabled check in more general way.
  // Missing this kind of check is likely to occur elsewhere if adding it in
  // each shadow element.
  HTMLInputElement* input = HostInput();
  if (!input || input->IsDisabledFormControl()) {
    StopDragging();
    HTMLDivElement::DefaultEventHandler(event);
    return;
  }

  auto& mouse_event = To<MouseEvent>(event);
  bool is_left_button =
      mouse_event.button() ==
      static_cast<int16_t>(WebPointerProperties::Button::kLeft);
  const AtomicString& event_type = event.type();

  // We intentionally do not call event->setDefaultHandled() here because
  // MediaControlTimelineElement::defaultEventHandler() wants to handle these
  // mouse events.
  if (event_type == event_type_names::kMousedown && is_left_button) {
    StartDragging();
    return;
  }
  if (event_type == event_type_names::kMouseup && is_left_button) {
    StopDragging();
    return;
  }
  if (event_type == event_type_names::kMousemove) {
    if (in_drag_mode_) {
      SetPositionFromPoint(
          PhysicalOffset::FromPointFFloor(mouse_event.AbsoluteLocation()));
    }
    return;
  }

  HTMLDivElement::DefaultEventHandler(event);
}

bool SliderThumbElement::WillRespondToMouseMoveEvents() const {
  const HTMLInputElement* input = HostInput();
  if (input && !input->IsDisabledFormControl() && in_drag_mode_)
    return true;

  return HTMLDivElement::WillRespondToMouseMoveEvents();
}

bool SliderThumbElement::WillRespondToMouseClickEvents() {
  const HTMLInputElement* input = HostInput();
  if (input && !input->IsDisabledFormControl())
    return true;

  return HTMLDivElement::WillRespondToMouseClickEvents();
}

void SliderThumbElement::DetachLayoutTree(bool performing_reattach) {
  if (in_drag_mode_) {
    if (LocalFrame* frame = GetDocument().GetFrame()) {
      frame->GetEventHandler().ReleasePointerCapture(
          PointerEventFactory::kMouseId, this);
    }
  }
  HTMLDivElement::DetachLayoutTree(performing_reattach);
}

HTMLInputElement* SliderThumbElement::HostInput() const {
  // Only HTMLInputElement creates SliderThumbElement instances as its shadow
  // nodes.  So, ownerShadowHost() must be an HTMLInputElement.
  return To<HTMLInputElement>(OwnerShadowHost());
}

const AtomicString& SliderThumbElement::ShadowPseudoId() const {
  HTMLInputElement* input = HostInput();
  if (!input || !input->GetLayoutObject())
    return shadow_element_names::kPseudoSliderThumb;

  const ComputedStyle& slider_style = input->GetLayoutObject()->StyleRef();
  switch (slider_style.EffectiveAppearance()) {
    case kMediaSliderPart:
    case kMediaSliderThumbPart:
    case kMediaVolumeSliderPart:
    case kMediaVolumeSliderThumbPart:
      return shadow_element_names::kPseudoMediaSliderThumb;
    default:
      return shadow_element_names::kPseudoSliderThumb;
  }
}

void SliderThumbElement::AdjustStyle(ComputedStyleBuilder& builder) {
  Element* host = OwnerShadowHost();
  DCHECK(host);
  const ComputedStyle& host_style = host->ComputedStyleRef();

  if (host_style.EffectiveAppearance() == kSliderVerticalPart &&
      RuntimeEnabledFeatures::
          NonStandardAppearanceValueSliderVerticalEnabled()) {
    builder.SetEffectiveAppearance(kSliderThumbVerticalPart);
  } else if (host_style.EffectiveAppearance() == kSliderHorizontalPart) {
    builder.SetEffectiveAppearance(kSliderThumbHorizontalPart);
  } else if (host_style.EffectiveAppearance() == kMediaSliderPart) {
    builder.SetEffectiveAppearance(kMediaSliderThumbPart);
  } else if (host_style.EffectiveAppearance() == kMediaVolumeSliderPart) {
    builder.SetEffectiveAppearance(kMediaVolumeSliderThumbPart);
  }
  if (builder.HasEffectiveAppearance())
    LayoutTheme::GetTheme().AdjustSliderThumbSize(builder);
}

// --------------------------------

SliderContainerElement::SliderContainerElement(Document& document)
    : HTMLDivElement(document) {
  UpdateTouchEventHandlerRegistry();
  SetHasCustomStyleCallbacks();
}

HTMLInputElement* SliderContainerElement::HostInput() const {
  return To<HTMLInputElement>(OwnerShadowHost());
}

LayoutObject* SliderContainerElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutFlexibleBox>(this);
}

void SliderContainerElement::DefaultEventHandler(Event& event) {
  if (auto* touch_event = DynamicTo<TouchEvent>(event)) {
    HandleTouchEvent(touch_event);
    return;
  }
}

void SliderContainerElement::HandleTouchEvent(TouchEvent* event) {
  HTMLInputElement* input = HostInput();
  if (!input || !input->UserAgentShadowRoot() ||
      input->IsDisabledFormControl() || !event) {
    return;
  }

  if (event->type() == event_type_names::kTouchend) {
    // TODO: Also do this for touchcancel?
    input->DispatchFormControlChangeEvent();
    event->SetDefaultHandled();
    sliding_direction_ = Direction::kNoMove;
    touch_started_ = false;
    return;
  }

  // The direction of this series of touch actions has been determined, which is
  // perpendicular to the slider, so no need to adjust the value.
  if (!CanSlide()) {
    return;
  }

  TouchList* touches = event->targetTouches();
  auto* thumb = To<SliderThumbElement>(
      GetTreeScope().getElementById(shadow_element_names::kIdSliderThumb));
  if (!thumb || !touches)
    return;

  if (touches->length() == 1) {
    if (event->type() == event_type_names::kTouchstart) {
      start_point_ = touches->item(0)->AbsoluteLocation();
      sliding_direction_ = Direction::kNoMove;
      touch_started_ = true;
      thumb->SetPositionFromPoint(start_point_);
    } else if (touch_started_) {
      PhysicalOffset current_point = touches->item(0)->AbsoluteLocation();
      if (sliding_direction_ == Direction::kNoMove) {
        // Still needs to update the direction.
        sliding_direction_ = GetDirection(current_point, start_point_);
      }

      // sliding_direction_ has been updated, so check whether it's okay to
      // slide again.
      if (CanSlide()) {
        thumb->SetPositionFromPoint(current_point);
        event->SetDefaultHandled();
      }
    }
  }
}

SliderContainerElement::Direction SliderContainerElement::GetDirection(
    const PhysicalOffset& point1,
    const PhysicalOffset& point2) {
  if (point1 == point2) {
    return Direction::kNoMove;
  }
  if ((point1.left - point2.left).Abs() >= (point1.top - point2.top).Abs()) {
    return Direction::kHorizontal;
  }
  return Direction::kVertical;
}

bool SliderContainerElement::CanSlide() {
  if (!HostInput() || !HostInput()->GetLayoutObject() ||
      !HostInput()->GetLayoutObject()->Style()) {
    return false;
  }
  const ComputedStyle* slider_style = HostInput()->GetLayoutObject()->Style();
  const TransformOperations& transforms = slider_style->Transform();
  int transform_size = transforms.size();
  if (transform_size > 0) {
    for (int i = 0; i < transform_size; ++i) {
      if (transforms.at(i)->GetType() == TransformOperation::kRotate ||
          transforms.at(i)->GetType() == TransformOperation::kRotateZ) {
        return true;
      }
    }
  }
  bool is_horizontal = GetComputedStyle()->IsHorizontalWritingMode();
  if ((sliding_direction_ == Direction::kVertical && is_horizontal) ||
      (sliding_direction_ == Direction::kHorizontal && !is_horizontal)) {
    return false;
  }
  return true;
}

const AtomicString& SliderContainerElement::ShadowPseudoId() const {
  if (!OwnerShadowHost() || !OwnerShadowHost()->GetLayoutObject())
    return shadow_element_names::kPseudoSliderContainer;

  const ComputedStyle& slider_style =
      OwnerShadowHost()->GetLayoutObject()->StyleRef();
  switch (slider_style.EffectiveAppearance()) {
    case kMediaSliderPart:
    case kMediaSliderThumbPart:
    case kMediaVolumeSliderPart:
    case kMediaVolumeSliderThumbPart:
      return shadow_element_names::kPseudoMediaSliderContainer;
    default:
      return shadow_element_names::kPseudoSliderContainer;
  }
}

void SliderContainerElement::UpdateTouchEventHandlerRegistry() {
  if (has_touch_event_handler_) {
    return;
  }
  if (GetDocument().GetPage() &&
      GetDocument().Lifecycle().GetState() < DocumentLifecycle::kStopping) {
    EventHandlerRegistry& registry =
        GetDocument().GetFrame()->GetEventHandlerRegistry();
    registry.DidAddEventHandler(
        *this, EventHandlerRegistry::kTouchStartOrMoveEventPassive);
    registry.DidAddEventHandler(*this, EventHandlerRegistry::kPointerEvent);
    has_touch_event_handler_ = true;
  }
}

void SliderContainerElement::DidMoveToNewDocument(Document& old_document) {
  UpdateTouchEventHandlerRegistry();
  HTMLElement::DidMoveToNewDocument(old_document);
}

void SliderContainerElement::RemoveAllEventListeners() {
  Node::RemoveAllEventListeners();
  has_touch_event_handler_ = false;
}

}  // namespace blink
