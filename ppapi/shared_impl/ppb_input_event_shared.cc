// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/shared_impl/ppb_input_event_shared.h"

#include <stddef.h>

#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var.h"

using ppapi::thunk::PPB_InputEvent_API;

namespace ppapi {

InputEventData::InputEventData()
    : is_filtered(false),
      event_type(PP_INPUTEVENT_TYPE_UNDEFINED),
      event_time_stamp(0.0),
      event_modifiers(0),
      mouse_button(PP_INPUTEVENT_MOUSEBUTTON_NONE),
      mouse_position(PP_MakePoint(0, 0)),
      mouse_click_count(0),
      mouse_movement(PP_MakePoint(0, 0)),
      wheel_delta(PP_MakeFloatPoint(0.0f, 0.0f)),
      wheel_ticks(PP_MakeFloatPoint(0.0f, 0.0f)),
      wheel_scroll_by_page(false),
      key_code(0),
      code(),
      character_text(),
      composition_target_segment(-1),
      composition_selection_start(0),
      composition_selection_end(0),
      touches(),
      changed_touches(),
      target_touches() {}

InputEventData::~InputEventData() {}

PPB_InputEvent_Shared::PPB_InputEvent_Shared(ResourceObjectType type,
                                             PP_Instance instance,
                                             const InputEventData& data)
    : Resource(type, instance), data_(data) {}

PPB_InputEvent_API* PPB_InputEvent_Shared::AsPPB_InputEvent_API() {
  return this;
}

const InputEventData& PPB_InputEvent_Shared::GetInputEventData() const {
  return data_;
}

PP_InputEvent_Type PPB_InputEvent_Shared::GetType() { return data_.event_type; }

PP_TimeTicks PPB_InputEvent_Shared::GetTimeStamp() {
  return data_.event_time_stamp;
}

uint32_t PPB_InputEvent_Shared::GetModifiers() { return data_.event_modifiers; }

PP_InputEvent_MouseButton PPB_InputEvent_Shared::GetMouseButton() {
  return data_.mouse_button;
}

PP_Point PPB_InputEvent_Shared::GetMousePosition() {
  return data_.mouse_position;
}

int32_t PPB_InputEvent_Shared::GetMouseClickCount() {
  return data_.mouse_click_count;
}

PP_Point PPB_InputEvent_Shared::GetMouseMovement() {
  return data_.mouse_movement;
}

PP_FloatPoint PPB_InputEvent_Shared::GetWheelDelta() {
  return data_.wheel_delta;
}

PP_FloatPoint PPB_InputEvent_Shared::GetWheelTicks() {
  return data_.wheel_ticks;
}

PP_Bool PPB_InputEvent_Shared::GetWheelScrollByPage() {
  return PP_FromBool(data_.wheel_scroll_by_page);
}

uint32_t PPB_InputEvent_Shared::GetKeyCode() { return data_.key_code; }

PP_Var PPB_InputEvent_Shared::GetCharacterText() {
  return StringVar::StringToPPVar(data_.character_text);
}

PP_Var PPB_InputEvent_Shared::GetCode() {
  return StringVar::StringToPPVar(data_.code);
}

uint32_t PPB_InputEvent_Shared::GetIMESegmentNumber() {
  if (data_.composition_segment_offsets.empty())
    return 0;
  return static_cast<uint32_t>(data_.composition_segment_offsets.size() - 1);
}

uint32_t PPB_InputEvent_Shared::GetIMESegmentOffset(uint32_t index) {
  if (index >= data_.composition_segment_offsets.size())
    return 0;
  return data_.composition_segment_offsets[index];
}

int32_t PPB_InputEvent_Shared::GetIMETargetSegment() {
  return data_.composition_target_segment;
}

void PPB_InputEvent_Shared::GetIMESelection(uint32_t* start, uint32_t* end) {
  if (start)
    *start = data_.composition_selection_start;
  if (end)
    *end = data_.composition_selection_end;
}

void PPB_InputEvent_Shared::AddTouchPoint(PP_TouchListType list,
                                          const PP_TouchPoint& point) {
  TouchPointWithTilt point_with_tilt{point, {0, 0}};
  switch (list) {
    case PP_TOUCHLIST_TYPE_TOUCHES:
      data_.touches.push_back(point_with_tilt);
      break;
    case PP_TOUCHLIST_TYPE_CHANGEDTOUCHES:
      data_.changed_touches.push_back(point_with_tilt);
      break;
    case PP_TOUCHLIST_TYPE_TARGETTOUCHES:
      data_.target_touches.push_back(point_with_tilt);
      break;
    default:
      break;
  }
}

uint32_t PPB_InputEvent_Shared::GetTouchCount(PP_TouchListType list) {
  switch (list) {
    case PP_TOUCHLIST_TYPE_TOUCHES:
      return static_cast<uint32_t>(data_.touches.size());
    case PP_TOUCHLIST_TYPE_CHANGEDTOUCHES:
      return static_cast<uint32_t>(data_.changed_touches.size());
    case PP_TOUCHLIST_TYPE_TARGETTOUCHES:
      return static_cast<uint32_t>(data_.target_touches.size());
  }

  return 0;
}

std::vector<TouchPointWithTilt>* PPB_InputEvent_Shared::GetTouchListByType(
    PP_TouchListType list) {
  switch (list) {
    case PP_TOUCHLIST_TYPE_TOUCHES:
      return &data_.touches;
    case PP_TOUCHLIST_TYPE_CHANGEDTOUCHES:
      return &data_.changed_touches;
    case PP_TOUCHLIST_TYPE_TARGETTOUCHES:
      return &data_.target_touches;
  }
  return nullptr;
}

TouchPointWithTilt* PPB_InputEvent_Shared::GetTouchByTypeAndId(
    PP_TouchListType list,
    uint32_t id) {
  std::vector<TouchPointWithTilt>* points = GetTouchListByType(list);
  if (!points)
    return nullptr;

  for (size_t i = 0; i < points->size(); i++) {
    if (points->at(i).touch.id == id)
      return &points->at(i);
  }

  return nullptr;
}

PP_TouchPoint PPB_InputEvent_Shared::GetTouchByIndex(PP_TouchListType list,
                                                     uint32_t index) {
  std::vector<TouchPointWithTilt>* points = GetTouchListByType(list);

  if (!points || index >= points->size()) {
    return PP_MakeTouchPoint();
  }
  return points->at(index).touch;
}

PP_TouchPoint PPB_InputEvent_Shared::GetTouchById(PP_TouchListType list,
                                                  uint32_t id) {
  TouchPointWithTilt* point = GetTouchByTypeAndId(list, id);
  if (!point)
    return PP_MakeTouchPoint();

  return point->touch;
}

PP_FloatPoint PPB_InputEvent_Shared::GetTouchTiltByIndex(PP_TouchListType list,
                                                         uint32_t index) {
  std::vector<TouchPointWithTilt>* points = GetTouchListByType(list);

  if (!points || index >= points->size())
    return PP_MakeFloatPoint(0, 0);

  return points->at(index).tilt;
}

PP_FloatPoint PPB_InputEvent_Shared::GetTouchTiltById(PP_TouchListType list,
                                                      uint32_t id) {
  TouchPointWithTilt* point = GetTouchByTypeAndId(list, id);
  if (!point)
    return PP_MakeFloatPoint(0, 0);

  return point->tilt;
}

// static
PP_Resource PPB_InputEvent_Shared::CreateIMEInputEvent(
    ResourceObjectType type,
    PP_Instance instance,
    PP_InputEvent_Type event_type,
    PP_TimeTicks time_stamp,
    struct PP_Var text,
    uint32_t segment_number,
    const uint32_t* segment_offsets,
    int32_t target_segment,
    uint32_t selection_start,
    uint32_t selection_end) {
  if (event_type != PP_INPUTEVENT_TYPE_IME_COMPOSITION_START &&
      event_type != PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE &&
      event_type != PP_INPUTEVENT_TYPE_IME_COMPOSITION_END &&
      event_type != PP_INPUTEVENT_TYPE_IME_TEXT)
    return 0;

  InputEventData data;
  data.event_type = event_type;
  data.event_time_stamp = time_stamp;
  if (text.type == PP_VARTYPE_STRING) {
    StringVar* text_str = StringVar::FromPPVar(text);
    if (!text_str)
      return 0;
    data.character_text = text_str->value();
  }
  data.composition_target_segment = target_segment;
  if (segment_number != 0) {
    data.composition_segment_offsets.assign(
        &segment_offsets[0], &segment_offsets[segment_number + 1]);
  }
  data.composition_selection_start = selection_start;
  data.composition_selection_end = selection_end;

  return (new PPB_InputEvent_Shared(type, instance, data))->GetReference();
}

// static
PP_Resource PPB_InputEvent_Shared::CreateKeyboardInputEvent(
    ResourceObjectType type,
    PP_Instance instance,
    PP_InputEvent_Type event_type,
    PP_TimeTicks time_stamp,
    uint32_t modifiers,
    uint32_t key_code,
    struct PP_Var character_text,
    struct PP_Var code) {
  if (event_type != PP_INPUTEVENT_TYPE_RAWKEYDOWN &&
      event_type != PP_INPUTEVENT_TYPE_KEYDOWN &&
      event_type != PP_INPUTEVENT_TYPE_KEYUP &&
      event_type != PP_INPUTEVENT_TYPE_CHAR)
    return 0;

  InputEventData data;
  data.event_type = event_type;
  data.event_time_stamp = time_stamp;
  data.event_modifiers = modifiers;
  data.key_code = key_code;
  if (character_text.type == PP_VARTYPE_STRING) {
    StringVar* text_str = StringVar::FromPPVar(character_text);
    if (!text_str)
      return 0;
    data.character_text = text_str->value();
  }
  if (code.type == PP_VARTYPE_STRING) {
    StringVar* code_str = StringVar::FromPPVar(code);
    if (!code_str)
      return 0;
    data.code = code_str->value();
  }

  return (new PPB_InputEvent_Shared(type, instance, data))->GetReference();
}

// static
PP_Resource PPB_InputEvent_Shared::CreateMouseInputEvent(
    ResourceObjectType type,
    PP_Instance instance,
    PP_InputEvent_Type event_type,
    PP_TimeTicks time_stamp,
    uint32_t modifiers,
    PP_InputEvent_MouseButton mouse_button,
    const PP_Point* mouse_position,
    int32_t click_count,
    const PP_Point* mouse_movement) {
  if (event_type != PP_INPUTEVENT_TYPE_MOUSEDOWN &&
      event_type != PP_INPUTEVENT_TYPE_MOUSEUP &&
      event_type != PP_INPUTEVENT_TYPE_MOUSEMOVE &&
      event_type != PP_INPUTEVENT_TYPE_MOUSEENTER &&
      event_type != PP_INPUTEVENT_TYPE_MOUSELEAVE)
    return 0;

  InputEventData data;
  data.event_type = event_type;
  data.event_time_stamp = time_stamp;
  data.event_modifiers = modifiers;
  data.mouse_button = mouse_button;
  data.mouse_position = *mouse_position;
  data.mouse_click_count = click_count;
  data.mouse_movement = *mouse_movement;

  return (new PPB_InputEvent_Shared(type, instance, data))->GetReference();
}

// static
PP_Resource PPB_InputEvent_Shared::CreateWheelInputEvent(
    ResourceObjectType type,
    PP_Instance instance,
    PP_TimeTicks time_stamp,
    uint32_t modifiers,
    const PP_FloatPoint* wheel_delta,
    const PP_FloatPoint* wheel_ticks,
    PP_Bool scroll_by_page) {
  InputEventData data;
  data.event_type = PP_INPUTEVENT_TYPE_WHEEL;
  data.event_time_stamp = time_stamp;
  data.event_modifiers = modifiers;
  data.wheel_delta = *wheel_delta;
  data.wheel_ticks = *wheel_ticks;
  data.wheel_scroll_by_page = PP_ToBool(scroll_by_page);

  return (new PPB_InputEvent_Shared(type, instance, data))->GetReference();
}

// static
PP_Resource PPB_InputEvent_Shared::CreateTouchInputEvent(
    ResourceObjectType type,
    PP_Instance instance,
    PP_InputEvent_Type event_type,
    PP_TimeTicks time_stamp,
    uint32_t modifiers) {
  if (event_type != PP_INPUTEVENT_TYPE_TOUCHSTART &&
      event_type != PP_INPUTEVENT_TYPE_TOUCHMOVE &&
      event_type != PP_INPUTEVENT_TYPE_TOUCHEND &&
      event_type != PP_INPUTEVENT_TYPE_TOUCHCANCEL)
    return 0;

  InputEventData data;
  data.event_type = event_type;
  data.event_time_stamp = time_stamp;
  data.event_modifiers = modifiers;
  return (new PPB_InputEvent_Shared(type, instance, data))->GetReference();
}

}  // namespace ppapi
