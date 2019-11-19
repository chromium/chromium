// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_input_location.h"

#include "base/logging.h"
#include "device/vr/test/test_hook.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform_util.h"

namespace device {

MockWMRInputLocation::MockWMRInputLocation(ControllerFrameData data)
    : data_(data) {
  DecomposeTransform(&decomposed_device_to_origin_,
                     PoseFrameDataToTransform(data.pose_data));
}

MockWMRInputLocation::~MockWMRInputLocation() = default;

bool MockWMRInputLocation::TryGetPosition(
    ABI::Windows::Foundation::Numerics::Vector3* position) const {
  DCHECK(position);
  if (!data_.pose_data.is_valid)
    return false;

  position->X = decomposed_device_to_origin_.translate[0];
  position->Y = decomposed_device_to_origin_.translate[1];
  position->Z = decomposed_device_to_origin_.translate[2];
  return true;
}

bool MockWMRInputLocation::TryGetOrientation(
    ABI::Windows::Foundation::Numerics::Quaternion* orientation) const {
  DCHECK(orientation);
  if (!data_.pose_data.is_valid)
    return false;

  orientation->X = decomposed_device_to_origin_.quaternion.x();
  orientation->Y = decomposed_device_to_origin_.quaternion.y();
  orientation->Z = decomposed_device_to_origin_.quaternion.z();
  orientation->W = decomposed_device_to_origin_.quaternion.w();
  return true;
}

bool MockWMRInputLocation::TryGetPositionAccuracy(
    ABI::Windows::UI::Input::Spatial::SpatialInteractionSourcePositionAccuracy*
        position_accuracy) const {
  return false;
}

}  // namespace device
