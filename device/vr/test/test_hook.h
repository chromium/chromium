// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_TEST_TEST_HOOK_H_
#define DEVICE_VR_TEST_TEST_HOOK_H_

#include <array>
#include <cstdint>

#include "base/check.h"
#include "base/component_export.h"
#include "device/vr/public/mojom/test/browser_test_interfaces.mojom.h"
#include "device/vr/public/mojom/test/controller_frame_data.h"
#include "device/vr/public/mojom/test/device_config.h"
#include "device/vr/public/mojom/test/view_data.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

// Tests may implement this, and register it to control behavior of VR runtime.
// Note that this interface may need to be queried from a different standalone
// shared library, and as such complex types should not be taken ownership of
// or returned. All types should either require simple or no destruction.
// Types from `browser_test_interfaces.mojom` should be typemapped to
// appropriate types unless only references or raw pointers are returned.
class VRTestHook {
 public:
  virtual void OnFrameSubmitted(const std::vector<ViewData>& frame_data,
                                const std::vector<LayerData>& layers) = 0;
  virtual DeviceConfig WaitGetDeviceConfig() = 0;
  virtual device_test::mojom::XRTestFrameDataPtr WaitGetFrameData() = 0;
  virtual device_test::mojom::EventData WaitGetEventData() = 0;
  virtual bool WaitGetCanCreateSession() = 0;
  virtual device::mojom::XRVisibilityMaskPtr WaitGetVisibilityMask(
      uint32_t view_index) = 0;

  virtual void AttachCurrentThread() = 0;
  virtual void DetachCurrentThread() = 0;
};

class COMPONENT_EXPORT(VR_TEST_HOOK) ServiceTestHook {
 public:
  virtual void SetTestHook(
      mojo::PendingRemote<device_test::mojom::XRTestHook> hook) = 0;
};

}  // namespace device

#endif  // DEVICE_VR_TEST_TEST_HOOK_H_
