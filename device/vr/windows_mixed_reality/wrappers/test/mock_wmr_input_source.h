// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_SOURCE_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_SOURCE_H_

#include "device/vr/test/test_hook.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_source.h"

namespace device {
class MockWMRController : public WMRController {
 public:
  MockWMRController() = default;
  ~MockWMRController() override = default;

  uint16_t ProductId() override;
  uint16_t VendorId() override;
};

class MockWMRInputSource : public WMRInputSource {
 public:
  MockWMRInputSource(ControllerFrameData data, unsigned int id);
  ~MockWMRInputSource() override;

  uint32_t Id() const override;
  ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceKind Kind()
      const override;
  bool IsPointingSupported() const override;
  std::unique_ptr<WMRController> Controller() const override;
  ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceHandedness
  Handedness() const override;

 private:
  ControllerFrameData data_;
  unsigned int id_;
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_SOURCE_H_
