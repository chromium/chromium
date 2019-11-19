// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_POINTER_POSE_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_POINTER_POSE_H_

#include "device/vr/test/test_hook.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_pointer_pose.h"

namespace device {

class MockWMRPointerPose : public WMRPointerPose {
 public:
  MockWMRPointerPose(ControllerFrameData data);
  ~MockWMRPointerPose() override;

  bool IsValid() const override;
  std::unique_ptr<WMRPointerSourcePose> TryGetInteractionSourcePose(
      const WMRInputSource* source) const override;
  ABI::Windows::Foundation::Numerics::Vector3 HeadForward() const override;

 private:
  ControllerFrameData data_;
  DISALLOW_COPY(MockWMRPointerPose);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_TEST_MOCK_WMR_POINTER_POSE_H_
