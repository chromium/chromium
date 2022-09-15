// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/ime_input_event_dev.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_IMEInputEvent_Dev_0_2>() {
  return PPB_IME_INPUT_EVENT_DEV_INTERFACE_0_2;
}

template <> const char* interface_name<PPB_IMEInputEvent_Dev_0_1>() {
  return PPB_IME_INPUT_EVENT_DEV_INTERFACE_0_1;
}

}  // namespace

// IMEInputEvent_Dev -------------------------------------------------------

IMEInputEvent_Dev::IMEInputEvent_Dev() : InputEvent() {
}

IMEInputEvent_Dev::IMEInputEvent_Dev(const InputEvent& event) : InputEvent() {
  bool is_ime_event = false;
  if (has_interface<PPB_IMEInputEvent_Dev_0_2>()) {
    if (get_interface<PPB_IMEInputEvent_Dev_0_2>()->IsIMEInputEvent(
        event.pp_resource())) {
      is_ime_event = true;
    }
  } else if (has_interface<PPB_IMEInputEvent_Dev_0_1>()) {
    if (get_interface<PPB_IMEInputEvent_Dev_0_1>()->IsIMEInputEvent(
        event.pp_resource())) {
      is_ime_event = true;
    }
  }

  if (is_ime_event) {
    Module::Get()->core()->AddRefResource(event.pp_resource());
    PassRefFromConstructor(event.pp_resource());
  }
}

IMEInputEvent_Dev::IMEInputEvent_Dev(
    const InstanceHandle& instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    const Var& text,
    const std::vector<uint32_t>& segment_offsets,
    int32_t target_segment,
    const std::pair<uint32_t, uint32_t>& selection) : InputEvent() {
  if (!has_interface<PPB_IMEInputEvent_Dev_0_2>())
    return;
  uint32_t dummy = 0;
  PassRefFromConstructor(get_interface<PPB_IMEInputEvent_Dev_0_2>()->Create(
      instance.pp_instance(), type, time_stamp, text.pp_var(),
      segment_offsets.empty() ? 0u :
          static_cast<uint32_t>(segment_offsets.size() - 1),
      segment_offsets.empty() ? &dummy : &segment_offsets[0],
      target_segment, selection.first, selection.second));
}


Var IMEInputEvent_Dev::GetText() const {
  if (has_interface<PPB_IMEInputEvent_Dev_0_2>()) {
    return Var(PASS_REF,
               get_interface<PPB_IMEInputEvent_Dev_0_2>()->GetText(
                   pp_resource()));
  } else if (has_interface<PPB_IMEInputEvent_Dev_0_1>()) {
    return Var(PASS_REF,
               get_interface<PPB_IMEInputEvent_Dev_0_1>()->GetText(
                   pp_resource()));
  }
  return Var();
}

uint32_t IMEInputEvent_Dev::GetSegmentNumber() const {
  if (has_interface<PPB_IMEInputEvent_Dev_0_2>()) {
    return get_interface<PPB_IMEInputEvent_Dev_0_2>()->GetSegmentNumber(
        pp_resource());
  } else if (has_interface<PPB_IMEInputEvent_Dev_0_1>()) {
    return get_interface<PPB_IMEInputEvent_Dev_0_1>()->GetSegmentNumber(
        pp_resource());
  }
  return 0;
}

uint32_t IMEInputEvent_Dev::GetSegmentOffset(uint32_t index) const {
  if (has_interface<PPB_IMEInputEvent_Dev_0_2>()) {
    return get_interface<PPB_IMEInputEvent_Dev_0_2>()->GetSegmentOffset(
        pp_resource(), index);
  } else if (has_interface<PPB_IMEInputEvent_Dev_0_1>()) {
    return get_interface<PPB_IMEInputEvent_Dev_0_1>()->GetSegmentOffset(
        pp_resource(), index);
  }
  return 0;
}

int32_t IMEInputEvent_Dev::GetTargetSegment() const {
  if (has_interface<PPB_IMEInputEvent_Dev_0_2>()) {
    return get_interface<PPB_IMEInputEvent_Dev_0_2>()->GetTargetSegment(
        pp_resource());
  } else if (has_interface<PPB_IMEInputEvent_Dev_0_1>()) {
    return get_interface<PPB_IMEInputEvent_Dev_0_1>()->GetTargetSegment(
        pp_resource());
  }
  return 0;
}

std::pair<uint32_t, uint32_t> IMEInputEvent_Dev::GetSelection() const {
  std::pair<uint32_t, uint32_t> range(0, 0);
  if (has_interface<PPB_IMEInputEvent_Dev_0_2>()) {
    get_interface<PPB_IMEInputEvent_Dev_0_2>()->GetSelection(pp_resource(),
                                                         &range.first,
                                                         &range.second);
  } else if (has_interface<PPB_IMEInputEvent_Dev_0_1>()) {
    get_interface<PPB_IMEInputEvent_Dev_0_1>()->GetSelection(pp_resource(),
                                                             &range.first,
                                                             &range.second);
  }
  return range;
}


}  // namespace pp
