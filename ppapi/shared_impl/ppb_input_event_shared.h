// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_INPUT_EVENT_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_INPUT_EVENT_SHARED_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_input_event_api.h"

namespace ppapi {

struct TouchPointWithTilt {
  PP_TouchPoint touch;
  PP_FloatPoint tilt;
};

// IF YOU ADD STUFF TO THIS CLASS
// ==============================
// Be sure to add it to the STRUCT_TRAITS at the top of ppapi_messages.h
struct PPAPI_SHARED_EXPORT InputEventData {
  InputEventData();
  ~InputEventData();

  // Internal-only value. Set to true when this input event is filtered, that
  // is, should be delivered synchronously. This is used by the proxy.
  bool is_filtered;

  PP_InputEvent_Type event_type;
  PP_TimeTicks event_time_stamp;
  uint32_t event_modifiers;

  PP_InputEvent_MouseButton mouse_button;
  PP_Point mouse_position;
  int32_t mouse_click_count;
  PP_Point mouse_movement;

  PP_FloatPoint wheel_delta;
  PP_FloatPoint wheel_ticks;
  bool wheel_scroll_by_page;

  uint32_t key_code;

  // The key event's |code| attribute as defined in:
  // http://www.w3.org/TR/uievents/
  std::string code;

  std::string character_text;

  std::vector<uint32_t> composition_segment_offsets;
  int32_t composition_target_segment;
  uint32_t composition_selection_start;
  uint32_t composition_selection_end;

  std::vector<TouchPointWithTilt> touches;
  std::vector<TouchPointWithTilt> changed_touches;
  std::vector<TouchPointWithTilt> target_touches;
};

// This simple class implements the PPB_InputEvent_API in terms of the
// shared InputEventData structure
class PPAPI_SHARED_EXPORT PPB_InputEvent_Shared
    : public Resource,
      public thunk::PPB_InputEvent_API {
 public:
  PPB_InputEvent_Shared(ResourceObjectType type,
                        PP_Instance instance,
                        const InputEventData& data);

  // Resource overrides.
  PPB_InputEvent_API* AsPPB_InputEvent_API() override;

  // PPB_InputEvent_API implementation.
  const InputEventData& GetInputEventData() const override;
  PP_InputEvent_Type GetType() override;
  PP_TimeTicks GetTimeStamp() override;
  uint32_t GetModifiers() override;
  PP_InputEvent_MouseButton GetMouseButton() override;
  PP_Point GetMousePosition() override;
  int32_t GetMouseClickCount() override;
  PP_Point GetMouseMovement() override;
  PP_FloatPoint GetWheelDelta() override;
  PP_FloatPoint GetWheelTicks() override;
  PP_Bool GetWheelScrollByPage() override;
  uint32_t GetKeyCode() override;
  PP_Var GetCharacterText() override;
  PP_Var GetCode() override;
  uint32_t GetIMESegmentNumber() override;
  uint32_t GetIMESegmentOffset(uint32_t index) override;
  int32_t GetIMETargetSegment() override;
  void GetIMESelection(uint32_t* start, uint32_t* end) override;
  void AddTouchPoint(PP_TouchListType list,
                     const PP_TouchPoint& point) override;
  uint32_t GetTouchCount(PP_TouchListType list) override;
  PP_TouchPoint GetTouchByIndex(PP_TouchListType list, uint32_t index) override;
  PP_TouchPoint GetTouchById(PP_TouchListType list, uint32_t id) override;
  PP_FloatPoint GetTouchTiltByIndex(PP_TouchListType list,
                                    uint32_t index) override;
  PP_FloatPoint GetTouchTiltById(PP_TouchListType list, uint32_t id) override;

  // Implementations for event creation.
  static PP_Resource CreateIMEInputEvent(ResourceObjectType type,
                                         PP_Instance instance,
                                         PP_InputEvent_Type event_type,
                                         PP_TimeTicks time_stamp,
                                         struct PP_Var text,
                                         uint32_t segment_number,
                                         const uint32_t* segment_offsets,
                                         int32_t target_segment,
                                         uint32_t selection_start,
                                         uint32_t selection_end);
  static PP_Resource CreateKeyboardInputEvent(ResourceObjectType type,
                                              PP_Instance instance,
                                              PP_InputEvent_Type event_type,
                                              PP_TimeTicks time_stamp,
                                              uint32_t modifiers,
                                              uint32_t key_code,
                                              struct PP_Var character_text,
                                              struct PP_Var code);
  static PP_Resource CreateMouseInputEvent(
      ResourceObjectType type,
      PP_Instance instance,
      PP_InputEvent_Type event_type,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      PP_InputEvent_MouseButton mouse_button,
      const PP_Point* mouse_position,
      int32_t click_count,
      const PP_Point* mouse_movement);
  static PP_Resource CreateWheelInputEvent(ResourceObjectType type,
                                           PP_Instance instance,
                                           PP_TimeTicks time_stamp,
                                           uint32_t modifiers,
                                           const PP_FloatPoint* wheel_delta,
                                           const PP_FloatPoint* wheel_ticks,
                                           PP_Bool scroll_by_page);
  static PP_Resource CreateTouchInputEvent(ResourceObjectType type,
                                           PP_Instance instance,
                                           PP_InputEvent_Type event_type,
                                           PP_TimeTicks time_stamp,
                                           uint32_t modifiers);

 private:
  // Helper function to get the touch list by the type.
  std::vector<TouchPointWithTilt>* GetTouchListByType(PP_TouchListType type);
  // Helper function to get touchpoint by the list type and touchpoint id.
  TouchPointWithTilt* GetTouchByTypeAndId(PP_TouchListType type, uint32_t id);

  InputEventData data_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PPB_InputEvent_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_INPUT_EVENT_SHARED_H_
