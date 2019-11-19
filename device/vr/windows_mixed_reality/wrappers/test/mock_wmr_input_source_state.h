// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_SOURCE_STATE_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_SOURCE_STATE_H_

#include "device/vr/test/test_hook.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_source_state.h"

namespace device {

class MockWMRInputSourceState : public WMRInputSourceState {
 public:
  MockWMRInputSourceState(ControllerFrameData data, unsigned int id);
  ~MockWMRInputSourceState() override;

  std::unique_ptr<WMRPointerPose> TryGetPointerPose(
      const WMRCoordinateSystem* origin) const override;
  std::unique_ptr<WMRInputSource> GetSource() const override;

  bool IsGrasped() const override;
  bool IsSelectPressed() const override;
  double SelectPressedValue() const override;

  bool SupportsControllerProperties() const override;

  bool IsThumbstickPressed() const override;
  bool IsTouchpadPressed() const override;
  bool IsTouchpadTouched() const override;
  double ThumbstickX() const override;
  double ThumbstickY() const override;
  double TouchpadX() const override;
  double TouchpadY() const override;

  std::unique_ptr<WMRInputLocation> TryGetLocation(
      const WMRCoordinateSystem* origin) const override;

 private:
  bool IsButtonPressed(XrButtonId id) const;
  ControllerFrameData data_;
  unsigned int id_;
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_SOURCE_STATE_H_
