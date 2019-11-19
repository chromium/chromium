// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_MANAGER_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_MANAGER_H_

#include "device/vr/test/test_hook.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_manager.h"

namespace device {

class MockWMRInputSourceEventArgs : public WMRInputSourceEventArgs {
 public:
  MockWMRInputSourceEventArgs(
      ControllerFrameData data,
      unsigned int id,
      ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind kind);
  ~MockWMRInputSourceEventArgs() override;

  ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind PressKind()
      const override;
  std::unique_ptr<WMRInputSourceState> State() const override;

 private:
  ControllerFrameData data_;
  unsigned int id_;
  ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind kind_;
};

class MockWMRInputManager : public WMRInputManager {
 public:
  MockWMRInputManager();
  ~MockWMRInputManager() override;

  std::vector<std::unique_ptr<WMRInputSourceState>>
  GetDetectedSourcesAtTimestamp(
      Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestamp>
          timestamp) override;

  std::unique_ptr<InputEventCallbackList::Subscription> AddPressedCallback(
      const InputEventCallback& cb) override;

  std::unique_ptr<InputEventCallbackList::Subscription> AddReleasedCallback(
      const InputEventCallback& cb) override;

 private:
  void MaybeNotifyCallbacks(const ControllerFrameData& data, unsigned int id);
  void HandleCallback(
      const ControllerFrameData& data,
      unsigned int id,
      uint64_t mask,
      ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind kind);
  ControllerFrameData last_frame_data_[kMaxTrackedDevices];

  InputEventCallbackList pressed_callback_list_;
  InputEventCallbackList released_callback_list_;
  DISALLOW_COPY_AND_ASSIGN(MockWMRInputManager);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_MANAGER_H_
