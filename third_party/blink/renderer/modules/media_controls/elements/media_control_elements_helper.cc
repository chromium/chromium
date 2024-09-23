// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_div_element.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_input_element.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_impl.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
bool MediaControlElementsHelper::IsUserInteractionEvent(const Event& event) {
  const AtomicString& type = event.type();
  return type == event_type_names::kPointerdown ||
         type == event_type_names::kPointerup ||
         type == event_type_names::kMousedown ||
         type == event_type_names::kMouseup ||
         type == event_type_names::kClick ||
         type == event_type_names::kDblclick ||
         type == event_type_names::kGesturetap || IsA<KeyboardEvent>(event) ||
         IsA<TouchEvent>(event);
}

// static
bool MediaControlElementsHelper::IsUserInteractionEventForSlider(
    const Event& event,
    LayoutObject* layout_object) {
  // It is unclear if this can be converted to isUserInteractionEvent(), since
  // mouse* events seem to be eaten during a drag anyway, see
  // https://crbug.com/516416.
  if (IsUserInteractionEvent(event))
    return true;

  // Some events are only captured during a slider drag.
  const HTMLInputElement* slider = nullptr;
  if (layout_object)
    slider = DynamicTo<HTMLInputElement>(layout_object->GetNode());
  // TODO(crbug.com/695459#c1): HTMLInputElement::IsDraggedSlider is incorrectly
  // false for drags that start from the track instead of the thumb.
  // Use SliderThumbElement::in_drag_mode_ and
  // SliderContainerElement::touch_started_ instead.
  if (slider && !slider->IsDraggedSlider())
    return false;

  const AtomicString& type = event.type();
  return type == event_type_names::kMouseover ||
         type == event_type_names::kMouseout ||
         type == event_type_names::kMousemove ||
         type == event_type_names::kPointerover ||
         type == event_type_names::kPointerout ||
         type == event_type_names::kPointermove;
}

// static
const HTMLMediaElement* MediaControlElementsHelper::ToParentMediaElement(
    const Node* node) {
  if (!node)
    return nullptr;
  const Node* shadow_host = node->OwnerShadowHost();
  if (!shadow_host)
    return nullptr;

  return DynamicTo<HTMLMediaElement>(shadow_host);
}

// static
HTMLDivElement* MediaControlElementsHelper::CreateDiv(const AtomicString& id,
                                                      ContainerNode* parent) {
  DCHECK(parent);
  auto* element = MakeGarbageCollected<HTMLDivElement>(parent->GetDocument());
  element->SetShadowPseudoId(id);
  parent->ParserAppendChild(element);
  return element;
}

// static
gfx::Size MediaControlElementsHelper::GetSizeOrDefault(
    const Element& element,
    const gfx::Size& default_size_in_dips) {
  LayoutBox* box = element.GetLayoutBox();
  if (!box)
    return default_size_in_dips;

  float zoom_factor = 1.0f;
  if (const LocalFrame* frame = element.GetDocument().GetFrame())
    zoom_factor = frame->LayoutZoomFactor();
  return gfx::Size(round(box->LogicalWidth() / zoom_factor),
                   round(box->LogicalHeight() / zoom_factor));
}

// static
HTMLDivElement* MediaControlElementsHelper::CreateDivWithId(
    const AtomicString& id,
    ContainerNode* parent) {
  DCHECK(parent);
  auto* element = MakeGarbageCollected<HTMLDivElement>(parent->GetDocument());
  element->SetIdAttribute(id);
  parent->ParserAppendChild(element);
  return element;
}

// static
void MediaControlElementsHelper::NotifyMediaControlAccessibleFocus(
    Element* element) {
  const HTMLMediaElement* media_element = ToParentMediaElement(element);
  if (!media_element || !media_element->GetMediaControls())
    return;

  static_cast<MediaControlsImpl*>(media_element->GetMediaControls())
      ->OnAccessibleFocus();
}

void MediaControlElementsHelper::NotifyMediaControlAccessibleBlur(
    Element* element) {
  const HTMLMediaElement* media_element = ToParentMediaElement(element);
  if (!media_element || !media_element->GetMediaControls())
    return;

  static_cast<MediaControlsImpl*>(media_element->GetMediaControls())
      ->OnAccessibleBlur();
}

}  // namespace blink
