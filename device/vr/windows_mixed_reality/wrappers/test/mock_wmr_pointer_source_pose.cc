// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_pointer_source_pose.h"

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_input_location.h"

namespace device {

MockWMRPointerSourcePose::MockWMRPointerSourcePose(ControllerFrameData data)
    : data_(data) {}

MockWMRPointerSourcePose::~MockWMRPointerSourcePose() = default;

bool MockWMRPointerSourcePose::IsValid() const {
  return data_.pose_data.is_valid;
}

ABI::Windows::Foundation::Numerics::Vector3 MockWMRPointerSourcePose::Position()
    const {
  // Providing the same position and orientation as the controller should be
  // valid and make it easy to actually point at things in tests if ever
  // necessary.
  ABI::Windows::Foundation::Numerics::Vector3 ret;
  MockWMRInputLocation loc(data_);
  loc.TryGetPosition(&ret);
  return ret;
}

ABI::Windows::Foundation::Numerics::Quaternion
MockWMRPointerSourcePose::Orientation() const {
  ABI::Windows::Foundation::Numerics::Quaternion ret;
  MockWMRInputLocation loc(data_);
  loc.TryGetOrientation(&ret);
  return ret;
}

}  // namespace device
