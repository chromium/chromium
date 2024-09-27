// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/pointer_event_factory.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_event_init.h"
#include "third_party/blink/renderer/core/events/pointer_event_util.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/pointer_type_names.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

namespace {

inline int ToInt(WebPointerProperties::PointerType t) {
  return static_cast<int>(t);
}

uint16_t ButtonToButtonsBitfield(WebPointerProperties::Button button) {
#define CASE_BUTTON_TO_BUTTONS(enumLabel)       \
  case WebPointerProperties::Button::enumLabel: \
    return static_cast<uint16_t>(WebPointerProperties::Buttons::enumLabel)

  switch (button) {
    CASE_BUTTON_TO_BUTTONS(kNoButton);
    CASE_BUTTON_TO_BUTTONS(kLeft);
    CASE_BUTTON_TO_BUTTONS(kRight);
    CASE_BUTTON_TO_BUTTONS(kMiddle);
    CASE_BUTTON_TO_BUTTONS(kBack);
    CASE_BUTTON_TO_BUTTONS(kForward);
    CASE_BUTTON_TO_BUTTONS(kEraser);
  }

#undef CASE_BUTTON_TO_BUTTONS

  NOTREACHED_IN_MIGRATION();
  return 0;
}

const AtomicString& PointerEventNameForEventType(WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::Type::kPointerDown:
      return event_type_names::kPointerdown;
    case WebInputEvent::Type::kPointerUp:
      return event_type_names::kPointerup;
    case WebInputEvent::Type::kPointerMove:
      return event_type_names::kPointermove;
    case WebInputEvent::Type::kPointerRawUpdate:
      return event_type_names::kPointerrawupdate;
    case WebInputEvent::Type::kPointerCancel:
      return event_type_names::kPointercancel;
    default:
      NOTREACHED_IN_MIGRATION();
      return g_empty_atom;
  }
}

float GetPointerEventPressure(float force, uint16_t buttons) {
  if (!buttons) {
    return 0;
  }
  if (std::isnan(force)) {
    return 0.5;
  }
  return force;
}

void UpdateCommonPointerEventInit(const WebPointerEvent& web_pointer_event,
                                  const gfx::PointF& last_global_position,
                                  LocalDOMWindow* dom_window,
                                  PointerEventInit* pointer_event_init) {
  // This function should not update attributes like pointerId, isPrimary,
  // and pointerType which is the same among the coalesced events and the
  // dispatched event.

  WebPointerEvent web_pointer_event_in_root_frame =
      web_pointer_event.WebPointerEventInRootFrame();

  MouseEvent::SetCoordinatesFromWebPointerProperties(
      web_pointer_event_in_root_frame, dom_window, pointer_event_init);
  if (!web_pointer_event.is_raw_movement_event &&
      (web_pointer_event.GetType() == WebInputEvent::Type::kPointerMove ||
       web_pointer_event.GetType() == WebInputEvent::Type::kPointerRawUpdate)) {
    float device_scale_factor = 1;

    // movementX/Y is type int for pointerevent, so we still need to truncated
    // the coordinates before calculate movement.
    pointer_event_init->setMovementX(
        base::saturated_cast<int>(web_pointer_event.PositionInScreen().x() *
                                  device_scale_factor) -
        base::saturated_cast<int>(last_global_position.x() *
                                  device_scale_factor));
    pointer_event_init->setMovementY(
        base::saturated_cast<int>(web_pointer_event.PositionInScreen().y() *
                                  device_scale_factor) -
        base::saturated_cast<int>(last_global_position.y() *
                                  device_scale_factor));
  }

  // If width/height is unknown we let PointerEventInit set it to 1.
  // See https://w3c.github.io/pointerevents/#dom-pointerevent-width
  //
  // For pointerup, even known width/height is ignored in favor of the default
  // value of 1.  See https://github.com/w3c/pointerevents/issues/225.
  if (web_pointer_event_in_root_frame.HasWidth() &&
      web_pointer_event_in_root_frame.HasHeight() &&
      web_pointer_event.GetType() != WebInputEvent::Type::kPointerUp) {
    float scale_factor = 1.0f;
    if (dom_window && dom_window->GetFrame()) {
      scale_factor = 1.0f / dom_window->GetFrame()->LayoutZoomFactor();
    }

    gfx::SizeF point_shape =
        gfx::ScaleSize(gfx::SizeF(web_pointer_event_in_root_frame.width,
                                  web_pointer_event_in_root_frame.height),
                       scale_factor);
    pointer_event_init->setWidth(point_shape.width());
    pointer_event_init->setHeight(point_shape.height());
  }
  pointer_event_init->setPressure(GetPointerEventPressure(
      web_pointer_event.force, pointer_event_init->buttons()));
  pointer_event_init->setTiltX(round(web_pointer_event.tilt_x));
  pointer_event_init->setTiltY(round(web_pointer_event.tilt_y));
  pointer_event_init->setAltitudeAngle(PointerEventUtil::AltitudeFromTilt(
      web_pointer_event.tilt_x, web_pointer_event.tilt_y));
  pointer_event_init->setAzimuthAngle(PointerEventUtil::AzimuthFromTilt(
      web_pointer_event.tilt_x, web_pointer_event.tilt_y));
  pointer_event_init->setTangentialPressure(
      web_pointer_event.tangential_pressure);
  pointer_event_init->setTwist(web_pointer_event.twist);
}

}  // namespace

// static
const AtomicString& PointerEventFactory::PointerTypeNameForWebPointPointerType(
    WebPointerProperties::PointerType type) {
  // TODO(mustaq): Fix when the spec starts supporting hovering erasers.
  // See spec https://github.com/w3c/pointerevents/issues/134
  switch (type) {
    case WebPointerProperties::PointerType::kUnknown:
      return g_empty_atom;
    case WebPointerProperties::PointerType::kTouch:
      return pointer_type_names::kTouch;
    case WebPointerProperties::PointerType::kPen:
      return pointer_type_names::kPen;
    case WebPointerProperties::PointerType::kMouse:
      return pointer_type_names::kMouse;
    default:
      DUMP_WILL_BE_NOTREACHED();
      return g_empty_atom;
  }
}

HeapVector<Member<PointerEvent>> PointerEventFactory::CreateEventSequence(
    const WebPointerEvent& web_pointer_event,
    const PointerEventInit* pointer_event_init,
    const Vector<WebPointerEvent>& event_list,
    LocalDOMWindow* view) {
  AtomicString type = PointerEventNameForEventType(web_pointer_event.GetType());
  HeapVector<Member<PointerEvent>> result;

  if (!event_list.empty()) {
    // Make a copy of LastPointerPosition so we can modify it after creating
    // each coalesced event.
    gfx::PointF last_global_position =
        GetLastPointerPosition(pointer_event_init->pointerId(),
                               event_list.front(), web_pointer_event.GetType());

    for (const auto& event : event_list) {
      DCHECK_EQ(web_pointer_event.id, event.id);
      DCHECK_EQ(web_pointer_event.GetType(), event.GetType());
      DCHECK_EQ(web_pointer_event.pointer_type, event.pointer_type);

      PointerEventInit* new_event_init = PointerEventInit::Create();
      if (pointer_event_init->hasButton()) {
        new_event_init->setButton(pointer_event_init->button());
      }
      if (pointer_event_init->hasButtons()) {
        new_event_init->setButtons(pointer_event_init->buttons());
      }
      if (pointer_event_init->hasIsPrimary()) {
        new_event_init->setIsPrimary(pointer_event_init->isPrimary());
      }
      if (pointer_event_init->hasPointerId()) {
        new_event_init->setPointerId(pointer_event_init->pointerId());
      }
      if (pointer_event_init->hasPointerType()) {
        new_event_init->setPointerType(pointer_event_init->pointerType());
      }
      if (pointer_event_init->hasView()) {
        new_event_init->setView(pointer_event_init->view());
      }

      new_event_init->setCancelable(false);
      new_event_init->setBubbles(false);
      UpdateCommonPointerEventInit(event, last_global_position, view,
                                   new_event_init);
      UIEventWithKeyState::SetFromWebInputEventModifiers(
          new_event_init,
          static_cast<WebInputEvent::Modifiers>(event.GetModifiers()));

      last_global_position = event.PositionInScreen();

      if (pointer_event_init->hasPersistentDeviceId()) {
        new_event_init->setPersistentDeviceId(
            pointer_event_init->persistentDeviceId());
      }

      PointerEvent* pointer_event =
          PointerEvent::Create(type, new_event_init, event.TimeStamp());
      // Set the trusted flag for these events at the creation time as oppose to
      // the normal events which is done at the dispatch time. This is because
      // we don't want to go over all these events at every dispatch and add the
      // implementation complexity while it has no sensible usecase at this
      // time.
      pointer_event->SetTrusted(true);
      result.push_back(pointer_event);
    }
  }
  return result;
}

const PointerId PointerEventFactory::kReservedNonPointerId = -1;
const PointerId PointerEventFactory::kInvalidId = 0;

// Mouse id is 1 to behave the same as MS Edge for compatibility reasons.
const PointerId PointerEventFactory::kMouseId = 1;

PointerEventInit* PointerEventFactory::ConvertIdTypeButtonsEvent(
    const WebPointerEvent& web_pointer_event) {
  WebPointerProperties::PointerType pointer_type =
      web_pointer_event.pointer_type;

  unsigned buttons;
  if (web_pointer_event.hovering) {
    buttons = MouseEvent::WebInputEventModifiersToButtons(
        static_cast<WebInputEvent::Modifiers>(
            web_pointer_event.GetModifiers()));
  } else {
    // TODO(crbug.com/816504): This is incorrect as we are assuming pointers
    // that don't hover have no other buttons except left which represents
    // touching the screen. This misconception comes from the touch devices and
    // is not correct for stylus.
    buttons = static_cast<unsigned>(
        (web_pointer_event.GetType() == WebInputEvent::Type::kPointerUp ||
         web_pointer_event.GetType() == WebInputEvent::Type::kPointerCancel)
            ? WebPointerProperties::Buttons::kNoButton
            : WebPointerProperties::Buttons::kLeft);
  }
  // Tweak the |buttons| to reflect pen eraser mode only if the pen is in
  // active buttons state w/o even considering the eraser button.
  // TODO(mustaq): Fix when the spec starts supporting hovering erasers.
  if (pointer_type == WebPointerProperties::PointerType::kEraser) {
    if (buttons != 0) {
      buttons |= static_cast<unsigned>(WebPointerProperties::Buttons::kEraser);
      buttons &= ~static_cast<unsigned>(WebPointerProperties::Buttons::kLeft);
    }
    pointer_type = WebPointerProperties::PointerType::kPen;
  }

  const IncomingId incoming_id(pointer_type, web_pointer_event.id);
  PointerId pointer_id = AddOrUpdateIdAndActiveButtons(
      incoming_id, buttons != 0, web_pointer_event.hovering,
      web_pointer_event.GetType(), web_pointer_event.unique_touch_event_id);
  if (pointer_id == kInvalidId) {
    return nullptr;
  }

  PointerEventInit* pointer_event_init = PointerEventInit::Create();
  pointer_event_init->setButtons(buttons);
  pointer_event_init->setPointerId(pointer_id);
  pointer_event_init->setPointerType(
      PointerTypeNameForWebPointPointerType(pointer_type));
  pointer_event_init->setIsPrimary(IsPrimary(pointer_id));

  return pointer_event_init;
}

void PointerEventFactory::SetEventSpecificFields(
    PointerEventInit* pointer_event_init,
    const AtomicString& type) {
  bool is_pointer_enter_or_leave = type == event_type_names::kPointerenter ||
                                   type == event_type_names::kPointerleave;
  pointer_event_init->setBubbles(!is_pointer_enter_or_leave);
  pointer_event_init->setCancelable(
      type != event_type_names::kPointerenter &&
      type != event_type_names::kPointerleave &&
      type != event_type_names::kPointercancel &&
      type != event_type_names::kPointerrawupdate &&
      type != event_type_names::kGotpointercapture &&
      type != event_type_names::kLostpointercapture);
  pointer_event_init->setComposed(!is_pointer_enter_or_leave);
  pointer_event_init->setDetail(0);
}

PointerEvent* PointerEventFactory::Create(
    const WebPointerEvent& web_pointer_event,
    const Vector<WebPointerEvent>& coalesced_events,
    const Vector<WebPointerEvent>& predicted_events,
    LocalDOMWindow* view) {
  const WebInputEvent::Type event_type = web_pointer_event.GetType();
  DCHECK(event_type == WebInputEvent::Type::kPointerDown ||
         event_type == WebInputEvent::Type::kPointerUp ||
         event_type == WebInputEvent::Type::kPointerMove ||
         event_type == WebInputEvent::Type::kPointerRawUpdate ||
         event_type == WebInputEvent::Type::kPointerCancel);

  PointerEventInit* pointer_event_init =
      ConvertIdTypeButtonsEvent(web_pointer_event);
  if (!pointer_event_init) {
    return nullptr;
  }

  AtomicString type = PointerEventNameForEventType(event_type);
  if (event_type == WebInputEvent::Type::kPointerDown ||
      event_type == WebInputEvent::Type::kPointerUp) {
    WebPointerProperties::Button button = web_pointer_event.button;
    // TODO(mustaq): Fix when the spec starts supporting hovering erasers.
    if (web_pointer_event.pointer_type ==
            WebPointerProperties::PointerType::kEraser &&
        button == WebPointerProperties::Button::kLeft) {
      button = WebPointerProperties::Button::kEraser;
    }
    pointer_event_init->setButton(static_cast<int>(button));

    // Make sure chorded buttons fire pointermove instead of pointerup/down.
    if ((event_type == WebInputEvent::Type::kPointerDown &&
         (pointer_event_init->buttons() & ~ButtonToButtonsBitfield(button)) !=
             0) ||
        (event_type == WebInputEvent::Type::kPointerUp &&
         pointer_event_init->buttons() != 0)) {
      type = event_type_names::kPointermove;
    }
  } else {
    pointer_event_init->setButton(
        static_cast<int16_t>(WebPointerProperties::Button::kNoButton));
  }

  pointer_event_init->setView(view);
  UpdateCommonPointerEventInit(
      web_pointer_event,
      GetLastPointerPosition(pointer_event_init->pointerId(), web_pointer_event,
                             event_type),
      view, pointer_event_init);

  UIEventWithKeyState::SetFromWebInputEventModifiers(
      pointer_event_init,
      static_cast<WebInputEvent::Modifiers>(web_pointer_event.GetModifiers()));

  SetEventSpecificFields(pointer_event_init, type);

  HeapVector<Member<PointerEvent>> coalesced_pointer_events,
      predicted_pointer_events;
  if (type == event_type_names::kPointermove ||
      type == event_type_names::kPointerrawupdate) {
    coalesced_pointer_events = CreateEventSequence(
        web_pointer_event, pointer_event_init, coalesced_events, view);
  }
  if (type == event_type_names::kPointermove) {
    predicted_pointer_events = CreateEventSequence(
        web_pointer_event, pointer_event_init, predicted_events, view);
  }
  pointer_event_init->setCoalescedEvents(coalesced_pointer_events);
  pointer_event_init->setPredictedEvents(predicted_pointer_events);

  SetLastPosition(pointer_event_init->pointerId(),
                  web_pointer_event.PositionInScreen(), event_type);

  pointer_event_init->setPersistentDeviceId(
      GetBlinkDeviceId(web_pointer_event));

  return PointerEvent::Create(
      type, pointer_event_init, web_pointer_event.TimeStamp(),
      MouseEvent::kRealOrIndistinguishable, kMenuSourceNone,
      web_pointer_event.GetPreventCountingAsInteraction());
}

void PointerEventFactory::SetLastPosition(int pointer_id,
                                          const gfx::PointF& position_in_screen,
                                          WebInputEvent::Type event_type) {
  PointerAttributes attributes = pointer_id_to_attributes_.Contains(pointer_id)
                                     ? pointer_id_to_attributes_.at(pointer_id)
                                     : PointerAttributes();

  if (event_type == WebInputEvent::Type::kPointerRawUpdate) {
    attributes.last_rawupdate_position = position_in_screen;
  } else {
    attributes.last_position = position_in_screen;
  }

  pointer_id_to_attributes_.Set(pointer_id, attributes);
}

void PointerEventFactory::RemoveLastPosition(const int pointer_id) {
  PointerAttributes attributes = pointer_id_to_attributes_.at(pointer_id);
  attributes.last_position.reset();
  attributes.last_rawupdate_position.reset();
  pointer_id_to_attributes_.Set(pointer_id, attributes);
}

gfx::PointF PointerEventFactory::GetLastPointerPosition(
    int pointer_id,
    const WebPointerProperties& event,
    WebInputEvent::Type event_type) const {
  if (event_type == WebInputEvent::Type::kPointerRawUpdate) {
    if (pointer_id_to_attributes_.Contains(pointer_id) &&
        pointer_id_to_attributes_.at(pointer_id)
            .last_rawupdate_position.has_value()) {
      return pointer_id_to_attributes_.at(pointer_id)
          .last_rawupdate_position.value();
    }
  } else {
    if (pointer_id_to_attributes_.Contains(pointer_id) &&
        pointer_id_to_attributes_.at(pointer_id).last_position.has_value()) {
      return pointer_id_to_attributes_.at(pointer_id).last_position.value();
    }
  }
  // If pointer_id is not in the map, returns the current position so the
  // movement will be zero.
  return event.PositionInScreen();
}

PointerEvent* PointerEventFactory::CreatePointerCancelEvent(
    const int pointer_id,
    base::TimeTicks platfrom_time_stamp,
    const int32_t device_id) {
  CHECK(pointer_id_to_attributes_.Contains(pointer_id));
  PointerAttributes attributes(pointer_id_to_attributes_.at(pointer_id));
  attributes.is_active_buttons = false;
  attributes.hovering = true;
  pointer_id_to_attributes_.Set(pointer_id, attributes);

  PointerEventInit* pointer_event_init = PointerEventInit::Create();

  pointer_event_init->setPointerId(pointer_id);
  pointer_event_init->setPointerType(PointerTypeNameForWebPointPointerType(
      pointer_id_to_attributes_.at(pointer_id).incoming_id.GetPointerType()));
  pointer_event_init->setIsPrimary(IsPrimary(pointer_id));

  SetEventSpecificFields(pointer_event_init, event_type_names::kPointercancel);

  pointer_event_init->setPersistentDeviceId(device_id);

  return PointerEvent::Create(event_type_names::kPointercancel,
                              pointer_event_init, platfrom_time_stamp);
}

PointerEvent* PointerEventFactory::CreatePointerEventFrom(
    PointerEvent* pointer_event,
    const AtomicString& type,
    EventTarget* related_target) {
  PointerEventInit* pointer_event_init = PointerEventInit::Create();

  pointer_event_init->setPointerId(pointer_event->pointerId());
  pointer_event_init->setPointerType(pointer_event->pointerType());
  pointer_event_init->setIsPrimary(pointer_event->isPrimary());
  pointer_event_init->setWidth(pointer_event->width());
  pointer_event_init->setHeight(pointer_event->height());
  pointer_event_init->setScreenX(pointer_event->screenX());
  pointer_event_init->setScreenY(pointer_event->screenY());
  pointer_event_init->setClientX(pointer_event->clientX());
  pointer_event_init->setClientY(pointer_event->clientY());
  pointer_event_init->setButton(pointer_event->button());
  pointer_event_init->setButtons(pointer_event->buttons());
  pointer_event_init->setPressure(pointer_event->pressure());
  pointer_event_init->setTiltX(pointer_event->tiltX());
  pointer_event_init->setTiltY(pointer_event->tiltY());
  pointer_event_init->setTangentialPressure(
      pointer_event->tangentialPressure());
  pointer_event_init->setTwist(pointer_event->twist());
  pointer_event_init->setView(pointer_event->view());
  pointer_event_init->setPersistentDeviceId(
      pointer_event->persistentDeviceId());

  SetEventSpecificFields(pointer_event_init, type);

  if (const UIEventWithKeyState* key_state_event =
          FindEventWithKeyState(pointer_event)) {
    UIEventWithKeyState::SetFromWebInputEventModifiers(
        pointer_event_init, key_state_event->GetModifiers());
  }

  if (related_target) {
    pointer_event_init->setRelatedTarget(related_target);
  }

  return PointerEvent::Create(type, pointer_event_init,
                              pointer_event->PlatformTimeStamp());
}

PointerEvent* PointerEventFactory::CreatePointerRawUpdateEvent(
    PointerEvent* pointer_event) {
  // This function is for creating pointerrawupdate event from a pointerdown/up
  // event that caused by chorded buttons and hence its type is changed to
  // pointermove.
  DCHECK(pointer_event->type() == event_type_names::kPointermove &&
         (pointer_event->buttons() &
          ~ButtonToButtonsBitfield(static_cast<WebPointerProperties::Button>(
              pointer_event->button()))) != 0 &&
         pointer_event->button() != 0);

  return CreatePointerEventFrom(pointer_event,
                                event_type_names::kPointerrawupdate,
                                pointer_event->relatedTarget());
}

PointerEvent* PointerEventFactory::CreatePointerCaptureEvent(
    PointerEvent* pointer_event,
    const AtomicString& type) {
  DCHECK(type == event_type_names::kGotpointercapture ||
         type == event_type_names::kLostpointercapture);

  return CreatePointerEventFrom(pointer_event, type,
                                pointer_event->relatedTarget());
}

PointerEvent* PointerEventFactory::CreatePointerBoundaryEvent(
    PointerEvent* pointer_event,
    const AtomicString& type,
    EventTarget* related_target) {
  DCHECK(type == event_type_names::kPointerout ||
         type == event_type_names::kPointerleave ||
         type == event_type_names::kPointerover ||
         type == event_type_names::kPointerenter);

  return CreatePointerEventFrom(pointer_event, type, related_target);
}

PointerEventFactory::PointerEventFactory() {
  Clear();
}

PointerEventFactory::~PointerEventFactory() {
  Clear();
}

void PointerEventFactory::Clear() {
  for (int type = 0;
       type <= ToInt(WebPointerProperties::PointerType::kMaxValue); type++) {
    primary_id_[type] = kInvalidId;
    id_count_[type] = 0;
  }
  pointer_incoming_id_mapping_.clear();
  pointer_id_to_attributes_.clear();
  recently_removed_pointers_.clear();

  device_id_browser_to_blink_mapping_.clear();

  // Always add mouse pointer in initialization and never remove it.
  // No need to add it to |pointer_incoming_id_mapping_| as it is not going to
  // be used with the existing APIs
  primary_id_[ToInt(WebPointerProperties::PointerType::kMouse)] = kMouseId;
  PointerAttributes attributes;
  attributes.incoming_id =
      IncomingId(WebPointerProperties::PointerType::kMouse, 0);
  pointer_id_to_attributes_.insert(kMouseId, attributes);

  current_id_ = PointerEventFactory::kMouseId + 1;
  current_device_id_ = 1;
  device_id_for_mouse_ = 0;
}

PointerId PointerEventFactory::AddOrUpdateIdAndActiveButtons(
    const IncomingId p,
    bool is_active_buttons,
    bool hovering,
    WebInputEvent::Type event_type,
    uint32_t unique_touch_event_id) {
  // Do not add extra mouse pointer as it was added in initialization.
  if (p.GetPointerType() == WebPointerProperties::PointerType::kMouse) {
    PointerAttributes attributes = pointer_id_to_attributes_.at(kMouseId);
    attributes.is_active_buttons = is_active_buttons;
    pointer_id_to_attributes_.Set(kMouseId, attributes);
    return kMouseId;
  }

  if (pointer_incoming_id_mapping_.Contains(p)) {
    PointerId mapped_id = pointer_incoming_id_mapping_.at(p);
    PointerAttributes attributes = pointer_id_to_attributes_.at(mapped_id);
    CHECK(attributes.incoming_id == p);
    attributes.is_active_buttons = is_active_buttons;
    attributes.hovering = hovering;
    pointer_id_to_attributes_.Set(mapped_id, attributes);
    return mapped_id;
  }

  // TODO(crbug.com/1141595): We should filter out bad pointercancel events
  // further upstream.
  if (event_type == WebInputEvent::Type::kPointerCancel) {
    return kInvalidId;
  }

  int type_int = p.PointerTypeInt();
  PointerId mapped_id = GetNextAvailablePointerid();
  if (!id_count_[type_int]) {
    primary_id_[type_int] = mapped_id;
  }
  id_count_[type_int]++;
  pointer_incoming_id_mapping_.insert(p, mapped_id);
  pointer_id_to_attributes_.insert(
      mapped_id,
      PointerAttributes(p, is_active_buttons, hovering, unique_touch_event_id,
                        /* last_position */ std::nullopt,
                        /* last_rawupdate_position */ std::nullopt));
  return mapped_id;
}

bool PointerEventFactory::Remove(const PointerId mapped_id) {
  // Do not remove mouse pointer id as it should always be there.
  if (mapped_id == kMouseId || !pointer_id_to_attributes_.Contains(mapped_id)) {
    return false;
  }

  IncomingId p = pointer_id_to_attributes_.at(mapped_id).incoming_id;
  int type_int = p.PointerTypeInt();
  PointerAttributes attributes = pointer_id_to_attributes_.Take(mapped_id);
  pointer_incoming_id_mapping_.erase(p);
  if (primary_id_[type_int] == mapped_id) {
    primary_id_[type_int] = kInvalidId;
  }
  id_count_[type_int]--;

  SaveRecentlyRemovedPointer(mapped_id, attributes);
  return true;
}

Vector<PointerId> PointerEventFactory::GetPointerIdsOfNonHoveringPointers()
    const {
  Vector<PointerId> mapped_ids;

  for (const auto& iter : pointer_id_to_attributes_) {
    if (!iter.value.hovering) {
      mapped_ids.push_back(static_cast<PointerId>(iter.key));
    }
  }

  // Sorting for a predictable ordering.
  std::sort(mapped_ids.begin(), mapped_ids.end());
  return mapped_ids;
}

bool PointerEventFactory::IsPrimary(PointerId mapped_id) const {
  if (!pointer_id_to_attributes_.Contains(mapped_id)) {
    return false;
  }

  IncomingId p = pointer_id_to_attributes_.at(mapped_id).incoming_id;
  return primary_id_[p.PointerTypeInt()] == mapped_id;
}

bool PointerEventFactory::IsActive(const PointerId pointer_id) const {
  return pointer_id_to_attributes_.Contains(pointer_id);
}

bool PointerEventFactory::IsPrimary(
    const WebPointerProperties& properties) const {
  // Mouse event is always primary.
  if (properties.pointer_type == WebPointerProperties::PointerType::kMouse) {
    return true;
  }

  // If !id_count, no pointer active, current WebPointerEvent will
  // be primary pointer when added to map.
  if (!id_count_[static_cast<int>(properties.pointer_type)]) {
    return true;
  }

  PointerId pointer_id = GetPointerEventId(properties);
  return (pointer_id != kInvalidId && IsPrimary(pointer_id));
}

bool PointerEventFactory::IsActiveButtonsState(
    const PointerId pointer_id) const {
  return pointer_id_to_attributes_.Contains(pointer_id) &&
         pointer_id_to_attributes_.at(pointer_id).is_active_buttons;
}

WebPointerProperties::PointerType PointerEventFactory::GetPointerType(
    PointerId pointer_id) const {
  if (!IsActive(pointer_id)) {
    return WebPointerProperties::PointerType::kUnknown;
  }
  return pointer_id_to_attributes_.at(pointer_id).incoming_id.GetPointerType();
}

PointerId PointerEventFactory::GetPointerEventId(
    const WebPointerProperties& properties) const {
  if (properties.pointer_type == WebPointerProperties::PointerType::kMouse) {
    return PointerEventFactory::kMouseId;
  }
  IncomingId incoming_id(properties.pointer_type, properties.id);
  if (pointer_incoming_id_mapping_.Contains(incoming_id)) {
    return pointer_incoming_id_mapping_.at(incoming_id);
  }
  return kInvalidId;
}

PointerId PointerEventFactory::GetPointerIdForTouchGesture(
    const uint32_t unique_touch_event_id) {
  const auto& unique_touch_id_matcher =
      [](const uint32_t unique_touch_event_id,
         const blink::PointerEventFactory::PointerAttributes attributes) {
        return attributes.incoming_id.GetPointerType() ==
                   WebPointerProperties::PointerType::kTouch &&
               attributes.unique_touch_event_id == unique_touch_event_id;
      };

  // First try currently active pointers.
  for (const auto& id : pointer_id_to_attributes_.Keys()) {
    if (unique_touch_id_matcher(unique_touch_event_id,
                                pointer_id_to_attributes_.at(id))) {
      return static_cast<PointerId>(id);
    }
  }

  // Then try recently removed pointers.
  for (const auto& id_attributes_pair : recently_removed_pointers_) {
    if (unique_touch_id_matcher(unique_touch_event_id,
                                id_attributes_pair.second)) {
      return static_cast<PointerId>(id_attributes_pair.first);
    }
  }

  // If the unique id is unseen, reserve a new pointer-id and save it as
  // recently removed.
  PointerId pointer_id = GetNextAvailablePointerid();
  PointerAttributes pointer_attributes;
  pointer_attributes.incoming_id =
      IncomingId(WebPointerProperties::PointerType::kTouch, kInvalidId);
  SaveRecentlyRemovedPointer(pointer_id, pointer_attributes);

  return pointer_id;
}

void PointerEventFactory::SaveRecentlyRemovedPointer(
    PointerId pointer_id,
    PointerAttributes pointer_attributes) {
  if (recently_removed_pointers_.size() == kRemovedPointersCapacity) {
    recently_removed_pointers_.pop_front();
  }
  recently_removed_pointers_.emplace_back(pointer_id, pointer_attributes);
}

int32_t PointerEventFactory::GetBlinkDeviceId(
    const WebPointerEvent& web_pointer_event) {
  if (web_pointer_event.pointer_type ==
      WebPointerProperties::PointerType::kMouse) {
    if (device_id_for_mouse_ == 0) {
      device_id_for_mouse_ = current_device_id_++;
    }
    return device_id_for_mouse_;
  }

  const int32_t incoming_id = web_pointer_event.device_id;
  // Invalid device id in browser is -1, however, an invalid uniqueId
  // is 0 as per the PointerEvent specification.
  if (incoming_id == -1) {
    return 0;
  }

  auto result = device_id_browser_to_blink_mapping_.insert(incoming_id, 0);
  if (result.is_new_entry) {
    result.stored_value->value = current_device_id_++;
  }
  TRACE_EVENT_INSTANT1("event", "PointerEventFactory::GetBlinkDeviceId",
                       TRACE_EVENT_SCOPE_THREAD, "id",
                       result.stored_value->value);
  return result.stored_value->value;
}

PointerId PointerEventFactory::GetNextAvailablePointerid() {
  // We do not handle the overflow of |current_id_| as it should be very rare.
  return current_id_++;
}

}  // namespace blink
