// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_input_manager.h"

#include <map>

#include "base/logging.h"
#include "device/vr/test/test_hook.h"
#include "device/vr/windows_mixed_reality/mixed_reality_statics.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_input_source_state.h"

namespace {

static const std::map<
    uint64_t,
    ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind>
    mask_to_press_kind = {
        // Select/trigger.
        {device::XrButtonMaskFromId(device::XrButtonId::kAxisTrigger),
         ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind_Select},
        // Menu.
        {device::XrButtonMaskFromId(device::XrButtonId::kMenu),
         ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind_Menu},
        // Grasp/grip.
        {device::XrButtonMaskFromId(device::XrButtonId::kGrip),
         ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind_Grasp},
        // Touchpad.
        {device::XrButtonMaskFromId(device::XrButtonId::kAxisTrackpad),
         ABI::Windows::UI::Input::Spatial::
             SpatialInteractionPressKind_Touchpad},
        // Joystick.
        {device::XrButtonMaskFromId(device::XrButtonId::kAxisThumbstick),
         ABI::Windows::UI::Input::Spatial::
             SpatialInteractionPressKind_Thumbstick},
};

}  // namespace

namespace device {

// MockWMRInputSourceEventArgs
MockWMRInputSourceEventArgs::MockWMRInputSourceEventArgs(
    ControllerFrameData data,
    unsigned int id,
    ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind kind)
    : data_(data), id_(id), kind_(kind) {}

MockWMRInputSourceEventArgs::~MockWMRInputSourceEventArgs() = default;

ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind
MockWMRInputSourceEventArgs::PressKind() const {
  return kind_;
}

std::unique_ptr<WMRInputSourceState> MockWMRInputSourceEventArgs::State()
    const {
  return std::make_unique<MockWMRInputSourceState>(data_, id_);
}

// MockWMRInputManager
MockWMRInputManager::MockWMRInputManager() {}

MockWMRInputManager::~MockWMRInputManager() = default;

std::vector<std::unique_ptr<WMRInputSourceState>>
MockWMRInputManager::GetDetectedSourcesAtTimestamp(
    Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestamp>
        timestamp) {
  std::vector<std::unique_ptr<WMRInputSourceState>> ret;
  auto locked_hook = MixedRealityDeviceStatics::GetLockedTestHook();
  if (!locked_hook.GetHook())
    return ret;

  // Index 0 should always be the headset, so start at 1 and keep going until
  // we stop getting valid controller data.
  for (unsigned int i = 1; i < kMaxTrackedDevices; ++i) {
    auto controller_data = locked_hook.GetHook()->WaitGetControllerData(i);
    if (!controller_data.is_valid)
      break;
    ret.push_back(
        std::make_unique<MockWMRInputSourceState>(controller_data, i));
    MaybeNotifyCallbacks(controller_data, i);
    last_frame_data_[i] = controller_data;
  }

  return ret;
}

std::unique_ptr<WMRInputManager::InputEventCallbackList::Subscription>
MockWMRInputManager::AddPressedCallback(
    const WMRInputManager::InputEventCallback& cb) {
  return pressed_callback_list_.Add(cb);
}

std::unique_ptr<WMRInputManager::InputEventCallbackList::Subscription>
MockWMRInputManager::AddReleasedCallback(
    const WMRInputManager::InputEventCallback& cb) {
  return released_callback_list_.Add(cb);
}

void MockWMRInputManager::MaybeNotifyCallbacks(const ControllerFrameData& data,
                                               unsigned int id) {
  DCHECK(id < kMaxTrackedDevices);
  // Determine if anything changed.
  ControllerFrameData last_frame_data = last_frame_data_[id];
  uint64_t changed_buttons =
      data.buttons_pressed ^ last_frame_data.buttons_pressed;
  if (changed_buttons == 0)
    return;

  // Determine which buttons changed - if more than one changed, we'll have to
  // fire the callbacks multiple times.
  for (const auto& pair : mask_to_press_kind) {
    // pair.first is the button mask for the button type being checked and
    // pair.second is the corresponding WMR SpatialInteractionPressKind. So, if
    // the button being checked changed, handle the correct callback with the
    // correct press kind.
    if (changed_buttons & pair.first) {
      HandleCallback(data, id, pair.first, pair.second);
    }
  }
}

void MockWMRInputManager::HandleCallback(
    const ControllerFrameData& data,
    unsigned int id,
    uint64_t mask,
    ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind kind) {
  bool now_pressed = ((data.buttons_pressed & mask) > 0);
  MockWMRInputSourceEventArgs args(data, id, kind);
  if (now_pressed) {
    pressed_callback_list_.Notify(args);
  } else {
    released_callback_list_.Notify(args);
  }
}

}  // namespace device
