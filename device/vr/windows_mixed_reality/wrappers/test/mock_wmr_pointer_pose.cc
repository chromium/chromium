// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_pointer_pose.h"

#include "base/logging.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_pointer_source_pose.h"

namespace device {

MockWMRPointerPose::MockWMRPointerPose(ControllerFrameData data)
    : data_(data) {}

MockWMRPointerPose::~MockWMRPointerPose() = default;

bool MockWMRPointerPose::IsValid() const {
  return data_.pose_data.is_valid;
}

std::unique_ptr<WMRPointerSourcePose>
MockWMRPointerPose::TryGetInteractionSourcePose(
    const WMRInputSource* source) const {
  return std::make_unique<MockWMRPointerSourcePose>(data_);
}

ABI::Windows::Foundation::Numerics::Vector3 MockWMRPointerPose::HeadForward()
    const {
  return {1, 0, 0};
}

}  // namespace device
