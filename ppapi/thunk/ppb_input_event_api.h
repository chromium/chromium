// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_INPUT_EVENT_API_H_
#define PPAPI_THUNK_PPB_INPUT_EVENT_API_H_

#include <stdint.h>

#include "ppapi/c/dev/ppb_ime_input_event_dev.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

struct InputEventData;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_InputEvent_API {
 public:
  virtual ~PPB_InputEvent_API() {}

  // This function is not exposed through the C API, but returns the internal
  // event data for easy proxying.
  virtual const InputEventData& GetInputEventData() const = 0;

  virtual PP_InputEvent_Type GetType() = 0;
  virtual PP_TimeTicks GetTimeStamp() = 0;
  virtual uint32_t GetModifiers() = 0;
  virtual PP_InputEvent_MouseButton GetMouseButton() = 0;
  virtual PP_Point GetMousePosition() = 0;
  virtual int32_t GetMouseClickCount() = 0;
  virtual PP_Point GetMouseMovement() = 0;
  virtual PP_FloatPoint GetWheelDelta() = 0;
  virtual PP_FloatPoint GetWheelTicks() = 0;
  virtual PP_Bool GetWheelScrollByPage() = 0;
  virtual uint32_t GetKeyCode() = 0;
  virtual PP_Var GetCharacterText() = 0;
  virtual PP_Var GetCode() = 0;
  virtual uint32_t GetIMESegmentNumber() = 0;
  virtual uint32_t GetIMESegmentOffset(uint32_t index) = 0;
  virtual int32_t GetIMETargetSegment() = 0;
  virtual void GetIMESelection(uint32_t* start, uint32_t* end) = 0;
  virtual void AddTouchPoint(PP_TouchListType list,
                             const PP_TouchPoint& point) = 0;
  virtual uint32_t GetTouchCount(PP_TouchListType list) = 0;
  virtual PP_TouchPoint GetTouchByIndex(PP_TouchListType list,
                                        uint32_t index) = 0;
  virtual PP_TouchPoint GetTouchById(PP_TouchListType list,
                                     uint32_t id) = 0;
  virtual PP_FloatPoint GetTouchTiltByIndex(PP_TouchListType list,
                                            uint32_t index) = 0;

  virtual PP_FloatPoint GetTouchTiltById(PP_TouchListType list,
                                         uint32_t id) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_INPUT_EVENT_API_H_
