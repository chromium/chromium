// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_TEST_TEST_HOOK_H_
#define DEVICE_VR_TEST_TEST_HOOK_H_

#include <array>
#include <cstdint>

#include "base/check.h"
#include "base/component_export.h"
#include "device/vr/public/mojom/browser_test_interfaces.mojom.h"
#include "device/vr/public/mojom/test_hook_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

// Tests may implement this, and register it to control behavior of VR runtime.
class VRTestHook {
 public:
  virtual void OnFrameSubmitted(const std::vector<ViewData>& frame_data) = 0;
  virtual DeviceConfig WaitGetDeviceConfig() = 0;
  virtual std::optional<gfx::Transform> WaitGetPresentingPose() = 0;
  virtual std::optional<gfx::Transform> WaitGetMagicWindowPose() = 0;
  virtual ControllerRole WaitGetControllerRoleForTrackedDeviceIndex(
      uint32_t index) = 0;
  virtual ControllerFrameData WaitGetControllerData(uint32_t index) = 0;
  virtual device_test::mojom::EventData WaitGetEventData() = 0;
  virtual bool WaitGetCanCreateSession() = 0;
  virtual std::optional<VisibilityMaskData> WaitGetVisibilityMask(
      uint32_t view_index) = 0;

  virtual void AttachCurrentThread() = 0;
  virtual void DetachCurrentThread() = 0;
};

class ServiceTestHook {
 public:
  virtual void SetTestHook(VRTestHook*) = 0;
};

}  // namespace device

#endif  // DEVICE_VR_TEST_TEST_HOOK_H_
