// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_LOCATION_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_LOCATION_H_

#include "device/vr/test/test_hook.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_location.h"
#include "ui/gfx/transform_util.h"

namespace device {

class MockWMRInputLocation : public WMRInputLocation {
 public:
  MockWMRInputLocation(ControllerFrameData data);
  ~MockWMRInputLocation() override;

  bool TryGetPosition(
      ABI::Windows::Foundation::Numerics::Vector3* position) const override;
  bool TryGetOrientation(ABI::Windows::Foundation::Numerics::Quaternion*
                             orientation) const override;
  bool TryGetPositionAccuracy(ABI::Windows::UI::Input::Spatial::
                                  SpatialInteractionSourcePositionAccuracy*
                                      position_accuracy) const override;

 private:
  ControllerFrameData data_;
  gfx::DecomposedTransform decomposed_device_to_origin_;
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_INPUT_LOCATION_H_
