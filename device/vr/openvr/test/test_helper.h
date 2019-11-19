// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENVR_TEST_TEST_HELPER_H_
#define DEVICE_VR_OPENVR_TEST_TEST_HELPER_H_

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "device/vr/test/test_hook.h"
#include "third_party/openvr/src/headers/openvr.h"

class ID3D11Texture2D;

namespace vr {

struct ProjectionRaw {
  float projection[4];
};

class TestHelper : public device::ServiceTestHook {
 public:
  // Methods called by mock OpenVR APIs.
  void OnPresentedFrame(ID3D11Texture2D* texture,
                        const VRTextureBounds_t* bounds,
                        EVREye eye);
  TrackedDevicePose_t GetPose(bool presenting);
  float GetInterpupillaryDistance();
  ProjectionRaw GetProjectionRaw(bool left);
  ETrackedPropertyError GetInt32TrackedDeviceProperty(
      unsigned int index,
      ETrackedDeviceProperty prop,
      int32_t& prop_value);
  ETrackedPropertyError GetUint64TrackedDeviceProperty(
      unsigned int index,
      ETrackedDeviceProperty prop,
      uint64_t& prop_value);
  ETrackedControllerRole GetControllerRoleForTrackedDeviceIndex(
      unsigned int index);
  ETrackedDeviceClass GetTrackedDeviceClass(unsigned int index);
  bool GetControllerState(unsigned int index,
                          VRControllerState_t* controller_state);
  bool GetControllerPose(unsigned int index,
                         TrackedDevicePose_t* controller_pose);
  void TestFailure();

  void AttachToCurrentThread();
  void DetachFromCurrentThread();

  // ServiceTestHook
  void SetTestHook(device::VRTestHook* hook) final;

 private:
  device::VRTestHook* test_hook_ GUARDED_BY(lock_) = nullptr;
  base::Lock lock_;
};

}  // namespace vr

#endif  // DEVICE_VR_OPENVR_TEST_TEST_HELPER_H_
