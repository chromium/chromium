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
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/layout/layout_slider_container.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"

namespace blink {

using namespace HTMLNames;

inline static bool HasVerticalAppearance(HTMLInputElement* input) {
  return input->ComputedStyleRef().Appearance() == kSliderVerticalPart;
}

inline SliderThumbElement::SliderThumbElement(Document& document)
    : HTMLDivElement(document), in_drag_mode_(false) {
  SetHasCustomStyleCallbacks();
}

SliderThumbElement* SliderThumbElement::Create(Document& document) {
  SliderThumbElement* element = new SliderThumbElement(document);
  element->setAttribute(idAttr, ShadowElementNames::SliderThumb());
  return element;
}

void SliderThumbElement::SetPositionFromValue() {
  // Since the code to calculate position is in the LayoutSliderThumb layout
  // path, we don't actually update the value here. Instead, we poke at the
  // layoutObject directly to trigger layout.
  if (GetLayoutObject())
    GetLayoutObject()->SetNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::kSliderValueChanged);
}

LayoutObject* SliderThumbElement::CreateLayoutObject(
    const ComputedStyle& style) {
  return LayoutObjectFactory::CreateBlockFlow(*this, style);
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

const Node* SliderThumbElement::FocusDelegate() const {
  return HostInput();
}

void SliderThumbElement::DragFrom(const LayoutPoint& point) {
  StartDragging();
  SetPositionFromPoint(point);
}

void SliderThumbElement::SetPositionFromPoint(const LayoutPoint& point) {
  HTMLInputElement* input(HostInput());
  Element* track_element = input->UserAgentShadowRoot()->getElementById(
      ShadowElementNames::SliderTrack());

  if (!input->GetLayoutObject() || !GetLayoutBox() ||
      !track_element->GetLayoutBox())
    return;

  LayoutPoint offset = LayoutPoint(input->GetLayoutObject()->AbsoluteToLocal(
      FloatPoint(point), kUseTransforms));
  bool is_vertical = HasVerticalAppearance(input);
  bool is_left_to_right_direction =
      GetLayoutBox()->Style()->IsLeftToRightDirection();
  LayoutUnit track_size;
  LayoutUnit position;
  LayoutUnit current_position;
  // We need to calculate currentPosition from absolute points becaue the
  // layoutObject for this node is usually on a layer and layoutBox()->x() and
  // y() are unusable.
  // FIXME: This should probably respect transforms.
  LayoutPoint absolute_thumb_origin =
      GetLayoutBox()->AbsoluteBoundingBoxRectIgnoringTransforms().Location();
  LayoutPoint absolute_slider_content_origin =
      LayoutPoint(input->GetLayoutObject()->LocalToAbsolute());
  IntRect track_bounding_box =
      track_element->GetLayoutObject()
          ->AbsoluteBoundingBoxRectIgnoringTransforms();
  IntRect input_bounding_box =
      input->GetLayoutObject()->AbsoluteBoundingBoxRectIgnoringTransforms();
  if (is_vertical) {
    track_size = track_element->GetLayoutBox()->ContentHeight() -
                 GetLayoutBox()->Size().Height();
    position = offset.Y() - GetLayoutBox()->Size().Height() / 2 -
               track_bounding_box.Y() + input_bounding_box.Y() -
               GetLayoutBox()->MarginBottom();
    current_position =
        absolute_thumb_origin.Y() - absolute_slider_content_origin.Y();
  } else {
    track_size = track_element->GetLayoutBox()->ContentWidth() -
                 GetLayoutBox()->Size().Width();
    position = offset.X() - GetLayoutBox()->Size().Width() / 2 -
               track_bounding_box.X() + input_bounding_box.X();
    position -= is_left_to_right_direction ? GetLayoutBox()->MarginLeft()
                                           : GetLayoutBox()->MarginRight();
    current_position =
        absolute_thumb_origin.X() - absolute_slider_content_origin.X();
  }
  position = std::min(position, track_size).ClampNegativeToZero();
  const Decimal ratio =
      Decimal::FromDouble(static_cast<double>(position) / track_size);
  const Decimal fraction =
      is_vertical || !is_left_to_right_direction ? Decimal(1) - ratio : ratio;
  StepRange step_range(input->CreateStepRange(kRejectAny));
  Decimal value =
      step_range.ClampValue(step_range.ValueFromProportion(fraction));

  Decimal closest = input->FindClosestTickMarkValue(value);
  if (closest.IsFinite()) {
    double closest_fraction =
        step_range.ProportionFromValue(closest).ToDouble();
    double closest_ratio = is_vertical || !is_left_to_right_direction
                               ? 1.0 - closest_fraction
                               : closest_fraction;
    LayoutUnit closest_position(track_size * closest_ratio);
    const LayoutUnit snapping_threshold(5);
    if ((closest_position - position).Abs() <= snapping_threshold)
      value = closest;
  }

  String value_string = SerializeForNumberType(value);
  if (value_string == input->value())
    return;

  // FIXME: This is no longer being set from renderer. Consider updating the
  // method name.
  input->SetValueFromRenderer(value_string);
  if (GetLayoutObject())
    GetLayoutObject()->SetNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::kSliderValueChanged);
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
  if (GetLayoutObject())
    GetLayoutObject()->SetNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::kSliderValueChanged);
  if (HostInput())
    HostInput()->DispatchFormControlChangeEvent();
}

void SliderThumbElement::DefaultEventHandler(Event& event) {
  if (event.IsPointerEvent() &&
      event.type() == EventTypeNames::lostpointercapture) {
    StopDragging();
    return;
  }

  if (!event.IsMouseEvent()) {
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

  auto& mouse_event = ToMouseEvent(event);
  bool is_left_button = mouse_event.button() ==
                        static_cast<short>(WebPointerProperties::Button::kLeft);
  const AtomicString& event_type = event.type();

  // We intentionally do not call event->setDefaultHandled() here because
  // MediaControlTimelineElement::defaultEventHandler() wants to handle these
  // mouse events.
  if (event_type == EventTypeNames::mousedown && is_left_button) {
    StartDragging();
    return;
  }
  if (event_type == EventTypeNames::mouseup && is_left_button) {
    StopDragging();
    return;
  }
  if (event_type == EventTypeNames::mousemove) {
    if (in_drag_mode_)
      SetPositionFromPoint(LayoutPoint(mouse_event.AbsoluteLocation()));
    return;
  }

  HTMLDivElement::DefaultEventHandler(event);
}

bool SliderThumbElement::WillRespondToMouseMoveEvents() {
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

void SliderThumbElement::DetachLayoutTree(const AttachContext& context) {
  if (in_drag_mode_) {
    if (LocalFrame* frame = GetDocument().GetFrame())
      frame->GetEventHandler().SetCapturingMouseEventsNode(nullptr);
  }
  HTMLDivElement::DetachLayoutTree(context);
}

HTMLInputElement* SliderThumbElement::HostInput() const {
  // Only HTMLInputElement creates SliderThumbElement instances as its shadow
  // nodes.  So, ownerShadowHost() must be an HTMLInputElement.
  return ToHTMLInputElement(OwnerShadowHost());
}

static const AtomicString& SliderThumbShadowPartId() {
  DEFINE_STATIC_LOCAL(const AtomicString, slider_thumb,
                      ("-webkit-slider-thumb"));
  return slider_thumb;
}

static const AtomicString& MediaSliderThumbShadowPartId() {
  DEFINE_STATIC_LOCAL(const AtomicString, media_slider_thumb,
                      ("-webkit-media-slider-thumb"));
  return media_slider_thumb;
}

const AtomicString& SliderThumbElement::ShadowPseudoId() const {
  HTMLInputElement* input = HostInput();
  if (!input || !input->GetLayoutObject())
    return SliderThumbShadowPartId();

  const ComputedStyle& slider_style = input->GetLayoutObject()->StyleRef();
  switch (slider_style.Appearance()) {
    case kMediaSliderPart:
    case kMediaSliderThumbPart:
    case kMediaVolumeSliderPart:
    case kMediaVolumeSliderThumbPart:
      return MediaSliderThumbShadowPartId();
    default:
      return SliderThumbShadowPartId();
  }
}

scoped_refptr<ComputedStyle> SliderThumbElement::CustomStyleForLayoutObject() {
  Element* host = OwnerShadowHost();
  DCHECK(host);
  const ComputedStyle& host_style = host->ComputedStyleRef();
  scoped_refptr<ComputedStyle> style = OriginalStyleForLayoutObject();

  if (host_style.Appearance() == kSliderVerticalPart)
    style->SetAppearance(kSliderThumbVerticalPart);
  else if (host_style.Appearance() == kSliderHorizontalPart)
    style->SetAppearance(kSliderThumbHorizontalPart);
  else if (host_style.Appearance() == kMediaSliderPart)
    style->SetAppearance(kMediaSliderThumbPart);
  else if (host_style.Appearance() == kMediaVolumeSliderPart)
    style->SetAppearance(kMediaVolumeSliderThumbPart);
  if (style->HasAppearance())
    LayoutTheme::GetTheme().AdjustSliderThumbSize(*style);

  return style;
}

// --------------------------------

inline SliderContainerElement::SliderContainerElement(Document& document)
    : HTMLDivElement(document),
      has_touch_event_handler_(false),
      touch_started_(false),
      sliding_direction_(kNoMove) {
  UpdateTouchEventHandlerRegistry();
  SetHasCustomStyleCallbacks();
}

DEFINE_NODE_FACTORY(SliderContainerElement)

HTMLInputElement* SliderContainerElement::HostInput() const {
  return ToHTMLInputElement(OwnerShadowHost());
}

LayoutObject* SliderContainerElement::CreateLayoutObject(const ComputedStyle&) {
  return new LayoutSliderContainer(this);
}

void SliderContainerElement::DefaultEventHandler(Event& event) {
  if (event.IsTouchEvent()) {
    HandleTouchEvent(ToTouchEvent(&event));
    return;
  }
}

void SliderContainerElement::HandleTouchEvent(TouchEvent* event) {
  HTMLInputElement* input = HostInput();
  if (!input || input->IsDisabledFormControl() || !event)
    return;

  if (event->type() == EventTypeNames::touchend) {
    // TODO: Also do this for touchcancel?
    input->DispatchFormControlChangeEvent();
    event->SetDefaultHandled();
    sliding_direction_ = kNoMove;
    touch_started_ = false;
    return;
  }

  // The direction of this series of touch actions has been determined, which is
  // perpendicular to the slider, so no need to adjust the value.
  if (!CanSlide()) {
    return;
  }

  TouchList* touches = event->targetTouches();
  SliderThumbElement* thumb = ToSliderThumbElement(
      GetTreeScope().getElementById(ShadowElementNames::SliderThumb()));
  if (!thumb || !touches)
    return;

  if (touches->length() == 1) {
    if (event->type() == EventTypeNames::touchstart) {
      start_point_ = touches->item(0)->AbsoluteLocation();
      sliding_direction_ = kNoMove;
      touch_started_ = true;
      thumb->SetPositionFromPoint(touches->item(0)->AbsoluteLocation());
    } else if (touch_started_) {
      LayoutPoint current_point = touches->item(0)->AbsoluteLocation();
      if (sliding_direction_ ==
          kNoMove) {  // Still needs to update the direction.
        sliding_direction_ = GetDirection(current_point, start_point_);
      }

      // sliding_direction_ has been updated, so check whether it's okay to
      // slide again.
      if (CanSlide()) {
        thumb->SetPositionFromPoint(touches->item(0)->AbsoluteLocation());
        event->SetDefaultHandled();
      }
    }
  }
}

SliderContainerElement::Direction SliderContainerElement::GetDirection(
    LayoutPoint& point1,
    LayoutPoint& point2) {
  if (point1 == point2) {
    return kNoMove;
  }
  if ((point1.X() - point2.X()).Abs() >= (point1.Y() - point2.Y()).Abs()) {
    return kHorizontal;
  }
  return kVertical;
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
      if (transforms.at(i)->GetType() == TransformOperation::kRotate) {
        return true;
      }
    }
  }
  if ((sliding_direction_ == kVertical &&
       slider_style->Appearance() == kSliderHorizontalPart) ||
      (sliding_direction_ == kHorizontal &&
       slider_style->Appearance() == kSliderVerticalPart)) {
    return false;
  }
  return true;
}

const AtomicString& SliderContainerElement::ShadowPseudoId() const {
  DEFINE_STATIC_LOCAL(const AtomicString, media_slider_container,
                      ("-webkit-media-slider-container"));
  DEFINE_STATIC_LOCAL(const AtomicString, slider_container,
                      ("-webkit-slider-container"));

  if (!OwnerShadowHost() || !OwnerShadowHost()->GetLayoutObject())
    return slider_container;

  const ComputedStyle& slider_style =
      OwnerShadowHost()->GetLayoutObject()->StyleRef();
  switch (slider_style.Appearance()) {
    case kMediaSliderPart:
    case kMediaSliderThumbPart:
    case kMediaVolumeSliderPart:
    case kMediaVolumeSliderThumbPart:
      return media_slider_container;
    default:
      return slider_container;
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

scoped_refptr<ComputedStyle>
SliderContainerElement::CustomStyleForLayoutObject() {
  HTMLInputElement* input = HostInput();
  DCHECK(input);
  scoped_refptr<ComputedStyle> style = OriginalStyleForLayoutObject();
  style->SetFlexDirection(HasVerticalAppearance(input) ? EFlexDirection::kColumn
                                                       : EFlexDirection::kRow);
  return style;
}

}  // namespace blink
